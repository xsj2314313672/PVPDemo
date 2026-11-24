// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPAnimBlueprintBuilder.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
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
#include "AnimGraphNode_Slot.h"
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
#include "Factories/AnimMontageFactory.h"
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
	const FName PVPAnimSlotSourcePinName(TEXT("Source"));

	/** Montage slot shared by the character and weapon animation blueprints (all migrated montages use it). */
	const FName PVPDefaultMontageSlotName(TEXT("DefaultSlot"));

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

	UAnimGraphNode_Slot* CreateSlotNode(UEdGraph* Graph, const FName SlotName, int32 PosX, int32 PosY,
		const FString& Context)
	{
		if (!Graph)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s: invalid graph"), *Context);
			return nullptr;
		}

		FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*Graph);
		UAnimGraphNode_Slot* Node = NodeCreator.CreateNode(false);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Node->Node.SlotName = SlotName;
		NodeCreator.Finalize();
		return Node;
	}

	UAnimationGraph* FindAnimGraph(UAnimBlueprint* AnimBlueprint)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return Cast<UAnimationGraph>(Graph);
			}
		}
		return nullptr;
	}

	UAnimGraphNode_Root* FindRootNode(UAnimationGraph* AnimGraph)
	{
		if (!AnimGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
			{
				return Root;
			}
		}
		return nullptr;
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
	UBlendSpace* AimBlendSpace,
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
	if (!Skeleton || !PreviewMesh || !GroundBlendSpace || !AimBlendSpace
		|| !RunLoop || !JumpStart || !JumpFallingLoop || !JumpEnd)
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
		|| AimBlendSpace->GetSkeleton() != Skeleton
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

	// Montage slot: locomotion montages (reload / fire / inspect) are authored against DefaultSlot,
	// so the state machine pose must flow through the slot before reaching the root.
	UAnimGraphNode_Slot* MontageSlotNode = CreateSlotNode(
		AnimGraph, PVPDefaultMontageSlotName, 720, 0, TEXT("montage slot"));
	if (!MontageSlotNode)
	{
		return nullptr;
	}

	if (!ConnectPins(AnimGraph,
		FindPin(StateMachineNode, PVPAnimPosePinName, EGPD_Output),
		MontageSlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input),
		TEXT("state machine -> montage slot")))
	{
		return nullptr;
	}

	if (!ConnectPins(AnimGraph,
		MontageSlotNode->FindPin(PVPAnimPosePinName, EGPD_Output),
		FindPin(RootNode, PVPAnimResultPinName, EGPD_Input),
		TEXT("montage slot -> root")))
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

	// Aim: a second blend space using the same direction plane, selected by bIsAiming.
	// UAnimNode_BlendListByBool selects index 0 when bActiveValue is true, so the aim pose goes to
	// BlendPose_0 and the standing / run result to BlendPose_1.
	UAnimGraphNode_BlendSpacePlayer* AimBlendSpaceNode = CreateBlendSpacePlayer(
		GroundGraph, AimBlendSpace, -620, 460, TEXT("aim blend space"));
	UAnimGraphNode_BlendListByBool* AimBlendNode = CreateBlendListByBool(GroundGraph, 160, 0);
	if (!AimBlendSpaceNode || !AimBlendNode)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create aim blend nodes"));
		return nullptr;
	}
	// BlendTime_0 applies when entering the aim pose, BlendTime_1 when returning to hip fire.
	if (UEdGraphPin* AimBlendTime0 = AimBlendNode->FindPin(TEXT("BlendTime_0"), EGPD_Input))
	{
		AimBlendTime0->DefaultValue = TEXT("0.15");
	}
	if (UEdGraphPin* AimBlendTime1 = AimBlendNode->FindPin(TEXT("BlendTime_1"), EGPD_Input))
	{
		AimBlendTime1->DefaultValue = TEXT("0.20");
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

	// The aimed set must describe the same direction plane as the standing set, otherwise the two
	// blend spaces would be sampled with different semantics while sharing one Input pin.
	const int32 AimAxisCount = AimBlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	if (AimAxisCount != AxisCount)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error,
			TEXT("aim blend space has %d axes, ground blend space has %d"), AimAxisCount, AxisCount);
		return nullptr;
	}

	TArray<FBlendAxisBinding> AimAxisBindings;
	AimAxisBindings.Reserve(AimAxisCount);
	for (int32 AxisIndex = 0; AxisIndex < AimAxisCount; ++AxisIndex)
	{
		const FBlendAxisBinding AimBinding = DescribeBlendAxis(AimBlendSpace, AxisIndex);
		const FBlendAxisBinding& GroundBinding = AxisBindings[AxisIndex];
		if (AimBinding.Source != GroundBinding.Source
			|| !FMath::IsNearlyEqual(AimBinding.Scale, GroundBinding.Scale))
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error,
				TEXT("aim blend space axis %d (source=%d scale=%.1f) does not match the ground axis "
					"(source=%d scale=%.1f)"),
				AxisIndex, static_cast<int32>(AimBinding.Source), AimBinding.Scale,
				static_cast<int32>(GroundBinding.Source), GroundBinding.Scale);
			return nullptr;
		}
		AimAxisBindings.Add(AimBinding);
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

	// The aimed blend space reuses the same Speed / Direction getters validated above.
	for (int32 AxisIndex = 0; AxisIndex < AimAxisBindings.Num(); ++AxisIndex)
	{
		const FBlendAxisBinding& Binding = AimAxisBindings[AxisIndex];
		const FName AxisPinName = AxisIndex == 0 ? TEXT("X") : TEXT("Y");
		UEdGraphPin* AxisPin = FindPin(AimBlendSpaceNode, AxisPinName, EGPD_Input);
		UEdGraphPin* ValuePin = BuildBlendAxisValuePin(
			GroundGraph, Binding, SpeedGetter, DirectionGetter,
			-1780 + AxisIndex * 620, 560 + AxisIndex * 260);
		if (!ConnectPins(GroundGraph, ValuePin, AxisPin,
			FString::Printf(TEXT("aim blend space %s"), *AxisPinName.ToString())))
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
			TEXT("blend space -> blend pose 1"))
		|| !ConnectPins(GroundGraph,
			FindPin(AimBlendSpaceNode, PVPAnimPosePinName, EGPD_Output),
			FindPin(AimBlendNode, TEXT("BlendPose_0"), EGPD_Input),
			TEXT("aim blend space -> aim blend pose 0"))
		|| !ConnectPins(GroundGraph,
			FindPin(RunBlendNode, PVPAnimPosePinName, EGPD_Output),
			FindPin(AimBlendNode, TEXT("BlendPose_1"), EGPD_Input),
			TEXT("standing blend -> aim blend pose 1")))
	{
		return nullptr;
	}

	UK2Node_CallFunction* RunningAndMovingNode = GroundGraph
		? CreateMathFunction(GroundGraph, GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), -560, 360)
		: nullptr;
	UK2Node_VariableGet* RunningGetter = CreateVariableGetter(GroundGraph, TEXT("bIsRunning"), -820, 320);
	UK2Node_VariableGet* MovingGetter = CreateVariableGetter(GroundGraph, TEXT("bIsMoving"), -820, 440);
	UK2Node_VariableGet* AimingGetter = CreateVariableGetter(GroundGraph, TEXT("bIsAiming"), 360, 200);
	if (!RunningAndMovingNode || !RunningGetter || !MovingGetter || !AimingGetter)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create ground run / aim condition nodes"));
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
			AimingGetter->GetValuePin(),
			FindPin(AimBlendNode, PVPAnimActiveValuePinName, EGPD_Input),
			TEXT("bIsAiming -> aim blend active value"))
		|| !ConnectPins(GroundGraph,
			FindPin(AimBlendNode, PVPAnimPosePinName, EGPD_Output),
			GroundState->GetPoseSinkPinInsideState(),
			TEXT("ground aim blend -> state result")))
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
	Wiring->SetBoolField(TEXT("montage_slot_source"),
		IsLinked(MontageSlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input)));
	Wiring->SetBoolField(TEXT("montage_slot_pose"),
		IsLinked(MontageSlotNode->FindPin(PVPAnimPosePinName, EGPD_Output)));
	Wiring->SetBoolField(TEXT("entry_to_ground"), IsLinked(GroundState->GetInputPin()));
	Wiring->SetBoolField(TEXT("ground_blendspace_x"),
		IsLinked(BlendSpaceNode->FindPin(TEXT("X"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_blendspace_y"),
		IsLinked(BlendSpaceNode->FindPin(TEXT("Y"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_run_blend"),
		IsLinked(RunBlendNode->FindPin(PVPAnimActiveValuePinName, EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_standing_pose"),
		IsLinked(RunBlendNode->FindPin(PVPAnimPosePinName, EGPD_Output)));
	Wiring->SetBoolField(TEXT("ground_aim_blend"),
		IsLinked(AimBlendNode->FindPin(PVPAnimActiveValuePinName, EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_aim_blendspace_x"),
		IsLinked(AimBlendSpaceNode->FindPin(TEXT("X"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_aim_blendspace_y"),
		IsLinked(AimBlendSpaceNode->FindPin(TEXT("Y"), EGPD_Input)));
	Wiring->SetBoolField(TEXT("ground_aim_pose"),
		IsLinked(AimBlendNode->FindPin(PVPAnimPosePinName, EGPD_Output)));
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
	Report->SetStringField(TEXT("aim_blendspace"), ObjectPathName(AimBlendSpace));
	Report->SetStringField(TEXT("montage_slot"), MontageSlotNode->Node.SlotName.ToString());
	Report->SetNumberField("blend_axis_count", AxisCount);
	Report->SetNumberField("aim_blend_axis_count", AimAxisCount);

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

	TArray<TSharedPtr<FJsonValue>> AimAxisRecords;
	for (const FBlendAxisBinding& Binding : AimAxisBindings)
	{
		TSharedPtr<FJsonObject> Axis = MakeShared<FJsonObject>();
		Axis->SetStringField("name", Binding.DisplayName);
		Axis->SetNumberField("min", Binding.Min);
		Axis->SetNumberField("max", Binding.Max);
		Axis->SetStringField(TEXT("bound_variable"), Binding.BoundVariable);
		AimAxisRecords.Add(MakeShared<FJsonValueObject>(Axis));
	}
	Report->SetArrayField("aim_blend_axes", AimAxisRecords);

	const auto AddSampleRecords = [](const UBlendSpace* BlendSpace)
	{
		TArray<TSharedPtr<FJsonValue>> Samples;
		for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
		{
			TSharedPtr<FJsonObject> SampleRecord = MakeShared<FJsonObject>();
			SampleRecord->SetStringField("animation", ObjectPathName(Sample.Animation));
			TArray<TSharedPtr<FJsonValue>> SampleValue;
			SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.X));
			SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.Y));
			SampleValue.Add(MakeShared<FJsonValueNumber>(Sample.SampleValue.Z));
			SampleRecord->SetArrayField("value", SampleValue);
			Samples.Add(MakeShared<FJsonValueObject>(SampleRecord));
		}
		return Samples;
	};
	Report->SetArrayField("blend_samples", AddSampleRecords(GroundBlendSpace));
	Report->SetArrayField("aim_blend_samples", AddSampleRecords(AimBlendSpace));

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

UAnimBlueprint* UPVPAnimBlueprintBuilder::BuildWeaponAnimBlueprint(
	const FString& AssetPath,
	USkeleton* Skeleton,
	USkeletalMesh* PreviewMesh,
	UAnimSequence* ReferencePose,
	FName MontageSlotName,
	const FString& ReportPath)
{
	if (!Skeleton || !PreviewMesh || !ReferencePose)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("missing skeleton / mesh / reference pose"));
		return nullptr;
	}
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("invalid package path: %s"), *AssetPath);
		return nullptr;
	}
	if (FindObject<UObject>(nullptr, *AssetPath) != nullptr)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error,
			TEXT("%s is still loaded; the Python driver must remove it first"), *AssetPath);
		return nullptr;
	}
	if (PreviewMesh->GetSkeleton() != Skeleton || ReferencePose->GetSkeleton() != Skeleton)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error,
			TEXT("weapon animation dependencies do not share the requested skeleton"));
		return nullptr;
	}

	if (MontageSlotName.IsNone())
	{
		MontageSlotName = PVPDefaultMontageSlotName;
	}

	const FString PackagePath = FPaths::GetPath(AssetPath);
	const FString AssetName = FPaths::GetCleanFilename(AssetPath);
	UE_LOG(LogPVPAnimBlueprintBuilder, Log, TEXT("creating weapon AnimBlueprint %s (slot=%s)"),
		*AssetPath, *MontageSlotName.ToString());

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->ParentClass = UAnimInstance::StaticClass();
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

	UAnimationGraph* AnimGraph = FindAnimGraph(AnimBlueprint);
	UAnimGraphNode_Root* RootNode = FindRootNode(AnimGraph);
	if (!AnimGraph || !RootNode)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s AnimGraph or root node is missing"), *AssetPath);
		return nullptr;
	}

	UAnimGraphNode_SequencePlayer* ReferenceNode = CreateSequencePlayer(
		AnimGraph, ReferencePose, true, -420, 0, TEXT("weapon reference pose"));
	UAnimGraphNode_Slot* SlotNode = CreateSlotNode(
		AnimGraph, MontageSlotName, 0, 0, TEXT("weapon montage slot"));
	if (!ReferenceNode || !SlotNode)
	{
		return nullptr;
	}

	if (!ConnectPins(AnimGraph,
		ReferenceNode->FindPin(PVPAnimPosePinName, EGPD_Output),
		SlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input),
		TEXT("reference pose -> montage slot")))
	{
		return nullptr;
	}

	if (!ConnectPins(AnimGraph,
		SlotNode->FindPin(PVPAnimPosePinName, EGPD_Output),
		RootNode->FindPin(PVPAnimResultPinName, EGPD_Input),
		TEXT("montage slot -> root")))
	{
		return nullptr;
	}

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
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("weapon AnimBlueprint compile failed (%d errors, %d warnings)"),
			CompileLog.NumErrors, CompileLog.NumWarnings);
		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("  %s"), *Message->ToText().ToString());
		}
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("asset_path"), AssetPath);
	Report->SetStringField(TEXT("parent_class"),
		AnimBlueprint->ParentClass ? AnimBlueprint->ParentClass->GetName() : TEXT("None"));
	Report->SetStringField(TEXT("skeleton"), ObjectPathName(AnimBlueprint->TargetSkeleton));
	Report->SetStringField(TEXT("preview_mesh"), ObjectPathName(PreviewMesh));
	Report->SetStringField(TEXT("reference_pose"), ObjectPathName(ReferencePose));
	Report->SetStringField(TEXT("montage_slot"), SlotNode->Node.SlotName.ToString());

	TSharedPtr<FJsonObject> Wiring = MakeShared<FJsonObject>();
	Wiring->SetBoolField(TEXT("reference_pose"),
		ReferenceNode->FindPin(PVPAnimPosePinName, EGPD_Output) != nullptr
		&& ReferenceNode->FindPin(PVPAnimPosePinName, EGPD_Output)->LinkedTo.Num() > 0);
	Wiring->SetBoolField(TEXT("slot_source"),
		SlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input) != nullptr
		&& SlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input)->LinkedTo.Num() > 0);
	Wiring->SetBoolField(TEXT("slot_pose"),
		SlotNode->FindPin(PVPAnimPosePinName, EGPD_Output) != nullptr
		&& SlotNode->FindPin(PVPAnimPosePinName, EGPD_Output)->LinkedTo.Num() > 0);
	Wiring->SetBoolField(TEXT("root_pose"),
		RootNode->FindPin(PVPAnimResultPinName, EGPD_Input) != nullptr
		&& RootNode->FindPin(PVPAnimResultPinName, EGPD_Input)->LinkedTo.Num() > 0);
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

bool UPVPAnimBlueprintBuilder::EnsureMontageSlot(
	const FString& AnimBlueprintPath,
	FName MontageSlotName,
	const FString& ReportPath)
{
	if (!FPackageName::IsValidLongPackageName(AnimBlueprintPath))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("invalid package path: %s"), *AnimBlueprintPath);
		return false;
	}

	if (MontageSlotName.IsNone())
	{
		MontageSlotName = PVPDefaultMontageSlotName;
	}

	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("cannot load %s"), *AnimBlueprintPath);
		return false;
	}

	UAnimationGraph* AnimGraph = FindAnimGraph(AnimBlueprint);
	UAnimGraphNode_Root* RootNode = FindRootNode(AnimGraph);
	UEdGraphPin* RootResultPin = RootNode ? RootNode->FindPin(PVPAnimResultPinName, EGPD_Input) : nullptr;
	if (!AnimGraph || !RootNode || !RootResultPin)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("%s AnimGraph / root / result pin is missing"),
			*AnimBlueprintPath);
		return false;
	}

	UAnimGraphNode_Slot* SlotNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Slot* Candidate = Cast<UAnimGraphNode_Slot>(Node))
		{
			if (Candidate->Node.SlotName == MontageSlotName)
			{
				SlotNode = Candidate;
				break;
			}
		}
	}

	const bool bFoundExisting = SlotNode != nullptr;
	bool bCreated = false;
	bool bWired = false;
	bool bCompiled = false;
	FCompilerResultsLog CompileLog;

	if (bFoundExisting)
	{
		UEdGraphPin* SlotSourcePin = SlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input);
		UEdGraphPin* SlotPosePin = SlotNode->FindPin(PVPAnimPosePinName, EGPD_Output);
		bWired = SlotSourcePin && SlotSourcePin->LinkedTo.Num() > 0
			&& SlotPosePin && SlotPosePin->LinkedTo.Contains(RootResultPin);
		if (!bWired)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error,
				TEXT("%s already has slot '%s' but it is not wired between its source and the root; "
					"delete and rebuild the asset instead."),
				*AnimBlueprintPath, *MontageSlotName.ToString());
		}
		else
		{
			// Idempotent re-run: the slot already exists and is wired, do not touch the asset.
			bCompiled = AnimBlueprint->Status == BS_UpToDate
				|| AnimBlueprint->Status == BS_UpToDateWithWarnings;
		}
	}
	else
	{
		UEdGraphPin* UpstreamPin = RootResultPin->LinkedTo.Num() > 0 ? RootResultPin->LinkedTo[0] : nullptr;
		if (!UpstreamPin)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error,
				TEXT("%s root has no upstream pose to insert slot '%s' into."),
				*AnimBlueprintPath, *MontageSlotName.ToString());
			return false;
		}

		SlotNode = CreateSlotNode(AnimGraph, MontageSlotName,
			RootNode->NodePosX - 320, RootNode->NodePosY, TEXT("ensure montage slot"));
		if (!SlotNode)
		{
			return false;
		}

		RootResultPin->BreakAllPinLinks(true);
		if (!ConnectPins(AnimGraph, UpstreamPin,
			SlotNode->FindPin(PVPAnimSlotSourcePinName, EGPD_Input),
			TEXT("ensure montage slot: upstream -> slot")))
		{
			return false;
		}
		if (!ConnectPins(AnimGraph,
			SlotNode->FindPin(PVPAnimPosePinName, EGPD_Output),
			RootResultPin,
			TEXT("ensure montage slot: slot -> root")))
		{
			return false;
		}

		bCreated = true;
		bWired = true;

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
		AnimBlueprint->GetOutermost()->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::None, &CompileLog);
		bCompiled = (AnimBlueprint->Status == BS_UpToDate
			|| AnimBlueprint->Status == BS_UpToDateWithWarnings)
			&& AnimBlueprint->GeneratedClass != nullptr
			&& CompileLog.NumErrors == 0;

		if (!bCompiled)
		{
			UE_LOG(LogPVPAnimBlueprintBuilder, Error,
				TEXT("%s slot insertion compile failed (%d errors, %d warnings)"),
				*AnimBlueprintPath, CompileLog.NumErrors, CompileLog.NumWarnings);
			for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
			{
				UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("  %s"), *Message->ToText().ToString());
			}
		}
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("asset_path"), AnimBlueprintPath);
	Report->SetStringField(TEXT("montage_slot"), MontageSlotName.ToString());
	Report->SetBoolField(TEXT("found_existing"), bFoundExisting);
	Report->SetBoolField(TEXT("created"), bCreated);
	Report->SetBoolField(TEXT("wired"), bWired);
	Report->SetBoolField(TEXT("compiled"), bCompiled);
	Report->SetNumberField(TEXT("compile_errors"), CompileLog.NumErrors);
	Report->SetNumberField(TEXT("compile_warnings"), CompileLog.NumWarnings);
	Report->SetBoolField(TEXT("generated_class"), AnimBlueprint->GeneratedClass != nullptr);

	if (!WriteJsonReport(ReportPath, Report))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Warning, TEXT("failed to write report to %s"), *ReportPath);
	}

	return bWired && bCompiled;
}

UAnimMontage* UPVPAnimBlueprintBuilder::BuildMontageFromSequence(
	const FString& AssetPath,
	UAnimSequence* SourceSequence,
	USkeleton* Skeleton,
	USkeletalMesh* PreviewMesh,
	FName MontageSlotName,
	const FString& ReportPath)
{
	if (!SourceSequence || !Skeleton || !PreviewMesh)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("missing sequence / skeleton / mesh for montage"));
		return nullptr;
	}
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("invalid package path: %s"), *AssetPath);
		return nullptr;
	}
	if (FindObject<UObject>(nullptr, *AssetPath) != nullptr)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error,
			TEXT("%s is still loaded; the Python driver must remove it first"), *AssetPath);
		return nullptr;
	}
	if (SourceSequence->GetSkeleton() != Skeleton || PreviewMesh->GetSkeleton() != Skeleton)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error,
			TEXT("montage dependencies do not share the requested skeleton"));
		return nullptr;
	}

	if (MontageSlotName.IsNone())
	{
		MontageSlotName = PVPDefaultMontageSlotName;
	}

	const FString PackagePath = FPaths::GetPath(AssetPath);
	const FString AssetName = FPaths::GetCleanFilename(AssetPath);
	UE_LOG(LogPVPAnimBlueprintBuilder, Log, TEXT("creating montage %s from %s (slot=%s)"),
		*AssetPath, *SourceSequence->GetName(), *MontageSlotName.ToString());

	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->SourceAnimation = SourceSequence;
	Factory->TargetSkeleton = Skeleton;
	Factory->PreviewSkeletalMesh = PreviewMesh;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimMontage::StaticClass(), Factory);
	UAnimMontage* Montage = Cast<UAnimMontage>(CreatedAsset);
	if (!Montage)
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Error, TEXT("failed to create montage %s"), *AssetPath);
		return nullptr;
	}

	// UAnimMontage's constructor already adds a DefaultSlot track; the factory fills it. Enforce the
	// requested name so a renamed slot cannot silently produce a montage the AnimGraph never plays.
	if (Montage->SlotAnimTracks.Num() == 0)
	{
		Montage->AddSlot(MontageSlotName);
	}
	else if (Montage->SlotAnimTracks[0].SlotName != MontageSlotName)
	{
		Montage->SlotAnimTracks[0].SlotName = MontageSlotName;
	}
	Montage->GetOutermost()->MarkPackageDirty();

	const int32 SegmentCount = Montage->SlotAnimTracks.Num() > 0
		? Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()
		: 0;
	bool bSourceSegmentMatches = false;
	if (Montage->SlotAnimTracks.Num() > 0)
	{
		for (const FAnimSegment& Segment : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			if (Segment.GetAnimReference() == SourceSequence)
			{
				bSourceSegmentMatches = true;
				break;
			}
		}
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("asset_path"), AssetPath);
	Report->SetStringField(TEXT("source_sequence"), ObjectPathName(SourceSequence));
	Report->SetStringField(TEXT("skeleton"), ObjectPathName(Montage->GetSkeleton()));
	Report->SetStringField(TEXT("preview_mesh"), ObjectPathName(Montage->GetPreviewMesh()));
	Report->SetStringField(TEXT("montage_slot"),
		Montage->SlotAnimTracks.Num() > 0 ? Montage->SlotAnimTracks[0].SlotName.ToString() : TEXT("None"));
	Report->SetNumberField(TEXT("slot_track_count"), Montage->SlotAnimTracks.Num());
	Report->SetNumberField(TEXT("segment_count"), SegmentCount);
	Report->SetNumberField(TEXT("length"), Montage->GetPlayLength());
	Report->SetNumberField(TEXT("section_count"), Montage->CompositeSections.Num());
	Report->SetStringField(TEXT("first_section"),
		Montage->CompositeSections.Num() > 0 ? Montage->CompositeSections[0].SectionName.ToString() : TEXT("None"));
	Report->SetBoolField(TEXT("source_segment_matches"), bSourceSegmentMatches);
	Report->SetBoolField(TEXT("slot_matches"),
		Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].SlotName == MontageSlotName);
	Report->SetBoolField(TEXT("skeleton_matches"), Montage->GetSkeleton() == Skeleton);

	if (!WriteJsonReport(ReportPath, Report))
	{
		UE_LOG(LogPVPAnimBlueprintBuilder, Warning, TEXT("failed to write report to %s"), *ReportPath);
	}

	return Montage;
}
