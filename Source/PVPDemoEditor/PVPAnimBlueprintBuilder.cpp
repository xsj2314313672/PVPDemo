// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPAnimBlueprintBuilder.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPAnimBlueprintBuilder, Log, All);

namespace
{
	const FName PVPAnimPosePinName(TEXT("Pose"));
	const FName PVPAnimResultPinName(TEXT("Result"));
	const FName PVPAnimTransitionRulePinName(TEXT("bCanEnterTransition"));
	const FName PVPAnimActiveValuePinName(TEXT("bActiveValue"));

	bool WriteJsonReport(const FString& ReportPath, const TSharedRef<FJsonObject>& Report)
	{
		if (ReportPath.IsEmpty())
		{
			return false;
		}

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Report, Writer))
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to serialize animation blueprint report"));
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		return FFileHelper::SaveStringToFile(Serialized, *ReportPath);
	}

	FString ObjectPathName(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	UEdGraphPin* FindPin(UEdGraphNode* Node, const FName PinName, EEdGraphPinDirection Direction)
	{
		return Node ? Node->FindPin(PinName, Direction) : nullptr;
	}

	bool ConnectPins(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, const FString& Context)
	{
		if (!Graph || !FromPin || !ToPin)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s: missing pin for connection"), *Context);
			return false;
		}

		const bool bConnected = Graph->GetSchema()->TryCreateConnection(FromPin, ToPin);
		const bool bLinked = FromPin->LinkedTo.Contains(ToPin) || ToPin->LinkedTo.Contains(FromPin);
		if (!bConnected || !bLinked)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error,
				TEXT("%s: failed to connect %s.%s -> %s.%s"),
				*Context,
				*FromPin->GetOwningNode()->GetName(), *FromPin->PinName.ToString(),
				*ToPin->GetOwningNode()->GetName(), *ToPin->PinName.ToString());
			return false;
		}
		return true;
	}

	UAnimGraphNode_SequencePlayer* CreateSequencePlayer(UEdGraph* Graph, UAnimSequence* Sequence, bool bLoop,
		int32 PosX, int32 PosY, const FString& Context)
	{
		if (!Graph || !Sequence)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s: invalid graph or sequence"), *Context);
			return nullptr;
		}

		FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*Graph);
		UAnimGraphNode_SequencePlayer* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Node->Node.SetSequence(Sequence);
		Node->Node.SetLoopAnimation(bLoop);
		Node->Node.SetPlayRate(1.0f);
		NodeCreator.Finalize();
		return Node;
	}

	UAnimGraphNode_BlendSpacePlayer* CreateBlendSpacePlayer(UEdGraph* Graph, UBlendSpace* BlendSpace,
		int32 PosX, int32 PosY, const FString& Context)
	{
		if (!Graph || !BlendSpace)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s: invalid graph or blend space"), *Context);
			return nullptr;
		}

		FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*Graph);
		UAnimGraphNode_BlendSpacePlayer* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Node->Node.SetBlendSpace(BlendSpace);
		Node->Node.SetLoop(true);
		NodeCreator.Finalize();
		Node->ReconstructNode();
		return Node;
	}

	UAnimGraphNode_BlendListByBool* CreateBlendListByBool(UEdGraph* Graph, int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<UAnimGraphNode_BlendListByBool> NodeCreator(*Graph);
		UAnimGraphNode_BlendListByBool* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		NodeCreator.Finalize();

		if (UEdGraphPin* BlendTime0 = Node->FindPin(TEXT("BlendTime_0"), EGPD_Input))
		{
			BlendTime0->DefaultValue = TEXT("0.20");
		}
		if (UEdGraphPin* BlendTime1 = Node->FindPin(TEXT("BlendTime_1"), EGPD_Input))
		{
			BlendTime1->DefaultValue = TEXT("0.20");
		}
		return Node;
	}

	UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FName VariableName, int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
		UK2Node_VariableGet* Node = NodeCreator.CreateNode(false);
		Node->VariableReference.SetSelfMember(VariableName);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		NodeCreator.Finalize();
		return Node;
	}

	UK2Node_CallFunction* CreateMathFunction(UEdGraph* Graph, const FName FunctionName, int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
		UK2Node_CallFunction* Node = NodeCreator.CreateNode(false);
		Node->FunctionReference.SetExternalMember(FunctionName, UKismetMathLibrary::StaticClass());
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		NodeCreator.Finalize();
		return Node;
	}

	UAnimStateNode* CreateState(UAnimationStateMachineGraph* Graph, const FString& StateName,
		int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<UAnimStateNode> NodeCreator(*Graph);
		UAnimStateNode* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		NodeCreator.Finalize();

		if (Node->BoundGraph)
		{
			FBlueprintEditorUtils::RenameGraphWithSuggestion(
				Node->BoundGraph, FNameValidatorFactory::MakeValidator(Node), StateName);
		}
		return Node;
	}

	UAnimStateTransitionNode* CreateTransition(UAnimationStateMachineGraph* Graph,
		UAnimStateNode* FromState, UAnimStateNode* ToState, float CrossfadeDuration, int32 PosX, int32 PosY)
	{
		FGraphNodeCreator<UAnimStateTransitionNode> NodeCreator(*Graph);
		UAnimStateTransitionNode* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		NodeCreator.Finalize();
		Node->CreateConnections(FromState, ToState);
		Node->CrossfadeDuration = CrossfadeDuration;
		return Node;
	}

	bool AddVariableTransitionRule(UAnimStateTransitionNode* Transition, const FName VariableName)
	{
		UAnimationTransitionGraph* TransitionGraph = Transition ? Cast<UAnimationTransitionGraph>(Transition->BoundGraph) : nullptr;
		UAnimGraphNode_TransitionResult* ResultNode = TransitionGraph ? TransitionGraph->GetResultNode() : nullptr;
		if (!TransitionGraph || !ResultNode)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("transition rule: missing transition graph or result node"));
			return false;
		}

		UK2Node_VariableGet* Getter = CreateVariableGetter(TransitionGraph, VariableName, -260, 0);
		UEdGraphPin* ValuePin = Getter ? Getter->GetValuePin() : nullptr;
		UEdGraphPin* RulePin = ResultNode->FindPin(PVPAnimTransitionRulePinName, EGPD_Input);
		return ConnectPins(TransitionGraph, ValuePin, RulePin, FString::Printf(TEXT("transition rule %s"), *VariableName.ToString()));
	}

	enum class EBlendAxisSource : uint8
	{
		Speed,
		Direction,
		DirectionSin,
		DirectionCos
	};

	struct FBlendAxisBinding
	{
		FString DisplayName;
		float Min = 0.0f;
		float Max = 0.0f;
		EBlendAxisSource Source = EBlendAxisSource::Speed;
		float Scale = 1.0f;
		FString BoundVariable;
	};

	FBlendAxisBinding DescribeBlendAxis(const UBlendSpace* BlendSpace, int32 AxisIndex)
	{
		FBlendAxisBinding Binding;
		const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(AxisIndex);
		Binding.DisplayName = Parameter.DisplayName;
		Binding.Min = Parameter.Min;
		Binding.Max = Parameter.Max;

		const FString LowerName = Parameter.DisplayName.ToLower();
		const float AxisExtent = FMath::Max(FMath::Abs(Parameter.Min), FMath::Abs(Parameter.Max));
		if (LowerName.Contains(TEXT("horizontal")) || LowerName == TEXT("x"))
		{
			// The migrated FP blend space is a direction plane: Horizontal = sin(Direction) * 100,
			// Vertical = cos(Direction) * 100 (samples at -100 / 0 / 100).
			Binding.Source = EBlendAxisSource::DirectionSin;
			Binding.Scale = AxisExtent > 0.0f ? AxisExtent : 1.0f;
			Binding.BoundVariable = TEXT("sin(Direction) * axis extent * bIsMoving");
		}
		else if (LowerName.Contains(TEXT("vertical")) || LowerName == TEXT("y"))
		{
			Binding.Source = EBlendAxisSource::DirectionCos;
			Binding.Scale = AxisExtent > 0.0f ? AxisExtent : 1.0f;
			Binding.BoundVariable = TEXT("cos(Direction) * axis extent * bIsMoving");
		}
		else if (LowerName.Contains(TEXT("speed")) || LowerName.Contains(TEXT("velocity")))
		{
			Binding.Source = EBlendAxisSource::Speed;
			Binding.BoundVariable = TEXT("Speed");
		}
		else if (LowerName.Contains(TEXT("direction"))
			|| LowerName.Contains(TEXT("angle"))
			|| LowerName.Contains(TEXT("yaw")))
		{
			Binding.Source = EBlendAxisSource::Direction;
			Binding.BoundVariable = TEXT("Direction");
		}
		else
		{
			// Display names are empty in some migrated blend spaces: an unsigned positive range is
			// Speed, a signed range is the direction angle.
			if (Parameter.Min >= 0.0f && Parameter.Max > 50.0f)
			{
				Binding.Source = EBlendAxisSource::Speed;
				Binding.BoundVariable = TEXT("Speed");
			}
			else if (Parameter.Min <= -45.0f && Parameter.Max >= 45.0f && AxisExtent > 110.0f)
			{
				Binding.Source = EBlendAxisSource::Direction;
				Binding.BoundVariable = TEXT("Direction");
			}
			else
			{
				Binding.Source = AxisIndex == 0 ? EBlendAxisSource::DirectionSin : EBlendAxisSource::DirectionCos;
				Binding.Scale = AxisExtent > 0.0f ? AxisExtent : 100.0f;
				Binding.BoundVariable = AxisIndex == 0
					? TEXT("sin(Direction) * axis extent * bIsMoving")
					: TEXT("cos(Direction) * axis extent * bIsMoving");
			}
		}
		return Binding;
	}

	UEdGraphPin* BuildBlendAxisValuePin(UEdGraph* Graph, const FBlendAxisBinding& Binding,
		UK2Node_VariableGet* SpeedGetter, UK2Node_VariableGet* DirectionGetter, int32 PosX, int32 PosY)
	{
		if (Binding.Source == EBlendAxisSource::Speed)
		{
			return SpeedGetter ? SpeedGetter->GetValuePin() : nullptr;
		}
		if (Binding.Source == EBlendAxisSource::Direction)
		{
			return DirectionGetter ? DirectionGetter->GetValuePin() : nullptr;
		}

		UEdGraphPin* DirectionPin = DirectionGetter ? DirectionGetter->GetValuePin() : nullptr;
		if (!Graph || !DirectionPin)
		{
			return nullptr;
		}

		const FName TrigFunction = Binding.Source == EBlendAxisSource::DirectionSin
			? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, DegSin)
			: GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, DegCos);
		UK2Node_CallFunction* TrigNode = CreateMathFunction(Graph, TrigFunction, PosX, PosY);
		// UKismetMathLibrary has no float*float function: use the reflected int*float overload with
		// the axis extent (100 for the Horizontal/Vertical plane) as the integer multiplier.
		UK2Node_CallFunction* ScaleNode = CreateMathFunction(
			Graph, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Multiply_IntFloat), PosX + 220, PosY);
		if (!TrigNode || !ScaleNode)
		{
			return nullptr;
		}

		if (!ConnectPins(Graph, DirectionPin, TrigNode->FindPin(TEXT("A"), EGPD_Input),
			TEXT("Direction -> trig"))
			|| !ConnectPins(Graph, TrigNode->GetReturnValuePin(), ScaleNode->FindPin(TEXT("B"), EGPD_Input),
				TEXT("trig -> scale")))
		{
			return nullptr;
		}

		if (UEdGraphPin* ScalePin = ScaleNode->FindPin(TEXT("A"), EGPD_Input))
		{
			ScalePin->DefaultValue = FString::FromInt(FMath::RoundToInt(Binding.Scale));
		}

		// Direction is 0 when standing still, which would evaluate to cos(0) * 100 = forward walk.
		// Gate both axes with bIsMoving so the blend space lands on its (0,0) Idle sample.
		UK2Node_VariableGet* MovingGetter = CreateVariableGetter(
			Graph, TEXT("bIsMoving"), PosX + 220, PosY + 200);
		UK2Node_CallFunction* GateNode = CreateMathFunction(
			Graph, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, SelectFloat), PosX + 460, PosY);
		if (!MovingGetter || !GateNode)
		{
			return nullptr;
		}
		if (!ConnectPins(Graph, ScaleNode->GetReturnValuePin(), GateNode->FindPin(TEXT("A"), EGPD_Input),
			TEXT("scale -> gate"))
			|| !ConnectPins(Graph, MovingGetter->GetValuePin(), GateNode->FindPin(TEXT("bPickA"), EGPD_Input),
				TEXT("bIsMoving -> gate")))
		{
			return nullptr;
		}
		if (UEdGraphPin* IdlePin = GateNode->FindPin(TEXT("B"), EGPD_Input))
		{
			IdlePin->DefaultValue = TEXT("0.0");
		}
		return GateNode->GetReturnValuePin();
	}
}

UAnimBlueprint* UPVPAnimBlueprintBuilder::BuildLocomotionAnimBlueprint(
	const FString& AssetPath,
	TSubclassOf<UAnimInstance> ParentClass,
	USkeleton* Skeleton,
	USkeletalMesh* PreviewMesh,
	UBlendSpace* GroundBlendSpace,
	UAnimSequence* RunLoop,
	UAnimSequence* JumpStart,
	UAnimSequence* JumpFallingLoop,
	UAnimSequence* JumpEnd,
	const FString& ReportPath)
{
	if (!ParentClass || !ParentClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("invalid AnimInstance parent class"));
		return nullptr;
	}
	if (!Skeleton || !PreviewMesh || !GroundBlendSpace || !RunLoop || !JumpStart || !JumpFallingLoop || !JumpEnd)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("missing skeleton / mesh / animation dependency"));
		return nullptr;
	}
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("invalid package path: %s"), *AssetPath);
		return nullptr;
	}
	// Do not use FPackageName::DoesPackageExist here: after a commandlet delete the package file can
	// remain visible until the process exits. A loaded asset object is the reliable conflict signal.
	if (FindObject<UObject>(nullptr, *AssetPath) != nullptr)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s is still loaded; the Python driver must remove it first"), *AssetPath);
		return nullptr;
	}

	if (GroundBlendSpace->GetSkeleton() != Skeleton
		|| RunLoop->GetSkeleton() != Skeleton
		|| JumpStart->GetSkeleton() != Skeleton
		|| JumpFallingLoop->GetSkeleton() != Skeleton
		|| JumpEnd->GetSkeleton() != Skeleton
		|| PreviewMesh->GetSkeleton() != Skeleton)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("animation dependencies do not share the requested skeleton"));
		return nullptr;
	}

	const FString PackagePath = FPaths::GetPath(AssetPath);
	const FString AssetName = FPaths::GetCleanFilename(AssetPath);
	UE_LOG(LogPVPAnimBlueprintBuilder, Log, TEXT("creating %s (parent=%s)"), *AssetPath, *ParentClass->GetName());

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->ParentClass = ParentClass;
	Factory->TargetSkeleton = Skeleton;
	Factory->PreviewSkeletalMesh = PreviewMesh;
	Factory->BlueprintType = BPTYPE_Normal;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(CreatedAsset);
	if (!AnimBlueprint)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create %s"), *AssetPath);
		return nullptr;
	}

	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			AnimGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s has no AnimGraph"), *AssetPath);
		return nullptr;
	}

	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		RootNode = Cast<UAnimGraphNode_Root>(Node);
		if (RootNode)
		{
			break;
		}
	}
	if (!RootNode)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s AnimGraph has no root node"), *AssetPath);
		return nullptr;
	}

	FGraphNodeCreator<UAnimGraphNode_StateMachine> StateMachineCreator(*AnimGraph);
	UAnimGraphNode_StateMachine* StateMachineNode = StateMachineCreator.CreateNode(false);
	StateMachineNode->NodePosX = 360;
	StateMachineNode->NodePosY = 0;
	StateMachineCreator.Finalize();

	UAnimationStateMachineGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("state machine node has no editor graph"));
		return nullptr;
	}

	if (!ConnectPins(AnimGraph,
		FindPin(StateMachineNode, PVPAnimPosePinName, EGPD_Output),
		FindPin(RootNode, PVPAnimResultPinName, EGPD_Input),
		TEXT("state machine -> root")))
	{
		return nullptr;
	}

	UAnimStateNode* GroundState = CreateState(StateMachineGraph, TEXT("Ground"), -320, 0);
	UAnimStateNode* JumpStartState = CreateState(StateMachineGraph, TEXT("JumpStart"), 80, -260);
	UAnimStateNode* FallingState = CreateState(StateMachineGraph, TEXT("Falling"), 420, -40);
	UAnimStateNode* LandState = CreateState(StateMachineGraph, TEXT("Land"), 760, 220);
	if (!GroundState || !JumpStartState || !FallingState || !LandState)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create one or more animation states"));
		return nullptr;
	}

	if (!ConnectPins(StateMachineGraph,
		StateMachineGraph->EntryNode ? StateMachineGraph->EntryNode->GetOutputPin() : nullptr,
		GroundState->GetInputPin(),
		TEXT("entry -> Ground")))
	{
		return nullptr;
	}

	// Ground: blend the Idle / Walk blend space with the run loop by (bIsRunning AND bIsMoving).
	UEdGraph* GroundGraph = GroundState->BoundGraph;
	UAnimGraphNode_BlendSpacePlayer* BlendSpaceNode = CreateBlendSpacePlayer(
		GroundGraph, GroundBlendSpace, -620, -160, TEXT("ground blend space"));
	UAnimGraphNode_SequencePlayer* RunNode = CreateSequencePlayer(
		GroundGraph, RunLoop, true, -620, 160, TEXT("run loop"));
	UAnimGraphNode_BlendListByBool* RunBlendNode = GroundGraph ? CreateBlendListByBool(GroundGraph, -220, 0) : nullptr;
	if (!GroundGraph || !BlendSpaceNode || !RunNode || !RunBlendNode)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create ground state nodes"));
		return nullptr;
	}

	const int32 AxisCount = GroundBlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	TArray<FBlendAxisBinding> AxisBindings;
	AxisBindings.Reserve(AxisCount);
	for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
	{
		AxisBindings.Add(DescribeBlendAxis(GroundBlendSpace, AxisIndex));
	}
	if (AxisBindings.Num() == 2
		&& AxisBindings[0].Source == EBlendAxisSource::Speed
		&& AxisBindings[1].Source == EBlendAxisSource::Speed)
	{
		// Keep the two axes independent when a migrated blend space has no display names.
		AxisBindings[1].Source = EBlendAxisSource::Direction;
		AxisBindings[1].BoundVariable = TEXT("Direction");
	}

	bool bNeedsSpeed = false;
	bool bNeedsDirection = false;
	for (const FBlendAxisBinding& Binding : AxisBindings)
	{
		if (Binding.Source == EBlendAxisSource::Speed)
		{
			bNeedsSpeed = true;
		}
		else
		{
			bNeedsDirection = true;
		}
	}
	UK2Node_VariableGet* SpeedGetter = bNeedsSpeed
		? CreateVariableGetter(GroundGraph, TEXT("Speed"), -980, -160)
		: nullptr;
	UK2Node_VariableGet* DirectionGetter = bNeedsDirection
		? CreateVariableGetter(GroundGraph, TEXT("Direction"), -980, 40)
		: nullptr;
	if ((bNeedsSpeed && !SpeedGetter) || (bNeedsDirection && !DirectionGetter))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create ground state variable getters"));
		return nullptr;
	}

	for (int32 AxisIndex = 0; AxisIndex < AxisBindings.Num(); ++AxisIndex)
	{
		const FBlendAxisBinding& Binding = AxisBindings[AxisIndex];
		const FName AxisPinName = AxisIndex == 0 ? TEXT("X") : TEXT("Y");
		UEdGraphPin* AxisPin = FindPin(BlendSpaceNode, AxisPinName, EGPD_Input);
		UEdGraphPin* ValuePin = BuildBlendAxisValuePin(
			GroundGraph, Binding, SpeedGetter, DirectionGetter,
			-1780 + AxisIndex * 620, -320 + AxisIndex * 260);
		if (!ConnectPins(GroundGraph, ValuePin, AxisPin,
			FString::Printf(TEXT("blend space %s"), *AxisPinName.ToString())))
		{
			return nullptr;
		}
	}

	if (!ConnectPins(GroundGraph,
		FindPin(RunNode, PVPAnimPosePinName, EGPD_Output),
		FindPin(RunBlendNode, TEXT("BlendPose_0"), EGPD_Input),
		TEXT("run -> blend pose 0"))
		|| !ConnectPins(GroundGraph,
			FindPin(BlendSpaceNode, PVPAnimPosePinName, EGPD_Output),
			FindPin(RunBlendNode, TEXT("BlendPose_1"), EGPD_Input),
			TEXT("blend space -> blend pose 1")))
	{
		return nullptr;
	}

	UK2Node_CallFunction* RunningAndMovingNode = GroundGraph
		? CreateMathFunction(GroundGraph, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), -560, 360)
		: nullptr;
	UK2Node_VariableGet* RunningGetter = CreateVariableGetter(GroundGraph, TEXT("bIsRunning"), -820, 320);
	UK2Node_VariableGet* MovingGetter = CreateVariableGetter(GroundGraph, TEXT("bIsMoving"), -820, 440);
	if (!RunningAndMovingNode || !RunningGetter || !MovingGetter)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create ground run condition nodes"));
		return nullptr;
	}
	if (!ConnectPins(GroundGraph,
			RunningGetter->GetValuePin(),
			RunningAndMovingNode->FindPin(TEXT("A"), EGPD_Input),
			TEXT("bIsRunning -> AND.A"))
		|| !ConnectPins(GroundGraph,
			MovingGetter->GetValuePin(),
			RunningAndMovingNode->FindPin(TEXT("B"), EGPD_Input),
			TEXT("bIsMoving -> AND.B"))
		|| !ConnectPins(GroundGraph,
			RunningAndMovingNode->GetReturnValuePin(),
			FindPin(RunBlendNode, PVPAnimActiveValuePinName, EGPD_Input),
			TEXT("AND -> blend active value"))
		|| !ConnectPins(GroundGraph,
			FindPin(RunBlendNode, PVPAnimPosePinName, EGPD_Output),
			GroundState->GetPoseSinkPinInsideState(),
			TEXT("ground blend -> state result")))
	{
		return nullptr;
	}

	// JumpStart / Falling / Land: one asset player per state.
	UAnimGraphNode_SequencePlayer* JumpStartNode = CreateSequencePlayer(
		JumpStartState->BoundGraph, JumpStart, false, -160, 0, TEXT("jump start"));
	UAnimGraphNode_SequencePlayer* FallingNode = CreateSequencePlayer(
		FallingState->BoundGraph, JumpFallingLoop, true, -160, 0, TEXT("jump falling loop"));
	UAnimGraphNode_SequencePlayer* LandNode = CreateSequencePlayer(
		LandState->BoundGraph, JumpEnd, false, -160, 0, TEXT("jump end"));
	if (!JumpStartNode || !FallingNode || !LandNode)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create jump state nodes"));
		return nullptr;
	}
	if (!ConnectPins(JumpStartState->BoundGraph,
			FindPin(JumpStartNode, PVPAnimPosePinName, EGPD_Output),
			JumpStartState->GetPoseSinkPinInsideState(),
			TEXT("jump start -> state result"))
		|| !ConnectPins(FallingState->BoundGraph,
			FindPin(FallingNode, PVPAnimPosePinName, EGPD_Output),
			FallingState->GetPoseSinkPinInsideState(),
			TEXT("falling -> state result"))
		|| !ConnectPins(LandState->BoundGraph,
			FindPin(LandNode, PVPAnimPosePinName, EGPD_Output),
			LandState->GetPoseSinkPinInsideState(),
			TEXT("land -> state result")))
	{
		return nullptr;
	}

	struct FTransitionSpec
	{
		UAnimStateNode* From = nullptr;
		UAnimStateNode* To = nullptr;
		FName VariableName;
		float Crossfade = 0.1f;
		int32 PosX = 0;
		int32 PosY = 0;
	};

	const FTransitionSpec TransitionSpecs[] =
	{
		{ GroundState, JumpStartState, TEXT("bIsJumping"), 0.10f, 160, -180 },
		{ GroundState, FallingState, TEXT("bIsFalling"), 0.10f, 220, 80 },
		{ JumpStartState, FallingState, TEXT("bIsFalling"), 0.10f, 520, -200 },
		{ JumpStartState, LandState, TEXT("bIsOnGround"), 0.08f, 560, 40 },
		{ FallingState, LandState, TEXT("bIsOnGround"), 0.10f, 820, 100 },
	};

	TArray<TSharedPtr<FJsonValue>> TransitionRecords;
	for (const FTransitionSpec& Spec : TransitionSpecs)
	{
		UAnimStateTransitionNode* Transition = CreateTransition(
			StateMachineGraph, Spec.From, Spec.To, Spec.Crossfade, Spec.PosX, Spec.PosY);
		if (!Transition || !AddVariableTransitionRule(Transition, Spec.VariableName))
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create transition %s -> %s"),
				*Spec.From->GetStateName(), *Spec.To->GetStateName());
			return nullptr;
		}

		TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField("from", Spec.From->GetStateName());
		Record->SetStringField("to", Spec.To->GetStateName());
		Record->SetStringField("rule", Spec.VariableName.ToString());
		Record->SetNumberField("crossfade", Spec.Crossfade);
		TransitionRecords.Add(MakeShared<FJsonValueObject>(Record));
	}

	UAnimStateTransitionNode* LandToGround = CreateTransition(
		StateMachineGraph, LandState, GroundState, 0.20f, 520, 320);
	if (!LandToGround)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create Land -> Ground transition"));
		return nullptr;
	}
	LandToGround->bAutomaticRuleBasedOnSequencePlayerInState = true;

	{
		TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField("from", LandState->GetStateName());
		Record->SetStringField("to", GroundState->GetStateName());
		Record->SetStringField("rule", TEXT("automatic: Jump_End remaining time"));
		Record->SetNumberField("crossfade", 0.20f);
		TransitionRecords.Add(MakeShared<FJsonValueObject>(Record));
	}

	const auto IsLinked = [](const UEdGraphPin* Pin)
	{
		return Pin != nullptr && Pin->LinkedTo.Num() > 0;
	};
	TSharedPtr<FJsonObject> Wiring = MakeShared<FJsonObject>();
	Wiring->SetBoolField(TEXT("root_pose"),
		IsLinked(RootNode->FindPin(PVPAnimResultPinName, EGPD_Input)));
	Wiring->SetBoolField(TEXT("state_machine_pose"),
		IsLinked(StateMachineNode->FindPin(PVPAnimPosePinName, EGPD_Output)));
	Wiring->SetBoolField(TEXT("entry_to_ground"), IsLinked(GroundState->GetInputPin()));
	Wiring->SetBoolField(TEXT("ground_blendspace_x"),
		IsLinked(BlendSpaceNode->FindPin(TEXT("X"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_blendspace_y"),
		IsLinked(BlendSpaceNode->FindPin(TEXT("Y"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_run_blend"),
		IsLinked(RunBlendNode->FindPin(PVPAnimActiveValuePinName, EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_pose"), IsLinked(GroundState->GetPoseSinkPinInsideState()));
	Wiring->SetBoolField(TEXT("jump_start_pose"), IsLinked(JumpStartState->GetPoseSinkPinInsideState()));
	Wiring->SetBoolField(TEXT("falling_pose"), IsLinked(FallingState->GetPoseSinkPinInsideState()));
	Wiring->SetBoolField(TEXT("land_pose"), IsLinked(LandState->GetPoseSinkPinInsideState()));
	Wiring->SetNumberField(TEXT("transition_rules"), UE_ARRAY_COUNT(TransitionSpecs));
	Wiring->SetBoolField(TEXT("land_auto_rule"),
		LandToGround->bAutomaticRuleBasedOnSequencePlayerInState);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	AnimBlueprint->GetOutermost()->MarkPackageDirty();

	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::None, &CompileLog);
	const bool bCompiled = (AnimBlueprint->Status == BS_UpToDate
		|| AnimBlueprint->Status == BS_UpToDateWithWarnings)
		&& AnimBlueprint->GeneratedClass != nullptr
		&& CompileLog.NumErrors == 0;

	if (!bCompiled)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("AnimBlueprint compile failed (%d errors, %d warnings)"),
			CompileLog.NumErrors, CompileLog.NumWarnings);
		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("  %s"), *Message->ToText().ToString());
		}
	}
	else
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Log, TEXT("AnimBlueprint compiled (%d warnings)"), CompileLog.NumWarnings);
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("asset_path"), AssetPath);
	Report->SetStringField(TEXT("parent_class"),
		AnimBlueprint->ParentClass ? AnimBlueprint->ParentClass->GetName() : TEXT("None"));
	Report->SetStringField(TEXT("skeleton"), ObjectPathName(AnimBlueprint->TargetSkeleton));
	Report->SetStringField(TEXT("preview_mesh"), ObjectPathName(PreviewMesh));
	Report->SetStringField(TEXT("ground_blendspace"), ObjectPathName(GroundBlendSpace));
	Report->SetNumberField("blend_axis_count", AxisCount);

	TArray<TSharedPtr<FJsonValue>> AxisRecords;
	for (const FBlendAxisBinding& Binding : AxisBindings)
	{
		TSharedPtr<FJsonObject> Axis = MakeShared<FJsonObject>();
		Axis->SetStringField("name", Binding.DisplayName);
		Axis->SetNumberField("min", Binding.Min);
		Axis->SetNumberField("max", Binding.Max);
		Axis->SetStringField(TEXT("bound_variable"), Binding.BoundVariable);
		AxisRecords.Add(MakeShared<FJsonValueObject>(Axis));
	}
	Report->SetArrayField("blend_axes", AxisRecords);

	TArray<TSharedPtr<FJsonValue>> SampleRecords;
	for (const FBlendSample& Sample : GroundBlendSpace->GetBlendSamples())
	{
		TSharedPtr<FJsonObject> SampleRecord = MakeShared<FJsonObject>();
		SampleRecord->SetStringField("animation", ObjectPathName(Sample.Animation));
		TArray<TSharedPtr<FJsonValue>> SampleValue;
		SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.X));
		SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.Y));
		SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.Z));
		SampleRecord->SetArrayField("value", SampleValue);
		SampleRecords.Add(MakeShared<FJsonValueObject>(SampleRecord));
	}
	Report->SetArrayField("blend_samples", SampleRecords);

	TArray<TSharedPtr<FJsonValue>> StateRecords;
	const TArray<UAnimStateNode*> States = { GroundState, JumpStartState, FallingState, LandState };
	for (UAnimStateNode* State : States)
	{
		TSharedPtr<FJsonObject> StateRecord = MakeShared<FJsonObject>();
		StateRecord->SetStringField("name", State->GetStateName());
		StateRecords.Add(MakeShared<FJsonValueObject>(StateRecord));
	}
	Report->SetArrayField(TEXT("states"), StateRecords);
	Report->SetArrayField(TEXT("transitions"), TransitionRecords);
	Report->SetObjectField(TEXT("wiring"), Wiring);
	Report->SetBoolField("compiled", bCompiled);
	Report->SetNumberField("compile_errors", CompileLog.NumErrors);
	Report->SetNumberField("compile_warnings", CompileLog.NumWarnings);
	Report->SetBoolField("generated_class", AnimBlueprint->GeneratedClass != nullptr);

	if (!WriteJsonReport(ReportPath, Report))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Warning, TEXT("failed to write report to %s"), *ReportPath);
	}

	return AnimBlueprint;
}
