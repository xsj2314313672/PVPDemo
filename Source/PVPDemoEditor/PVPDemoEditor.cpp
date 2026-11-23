// Copyright Epic Games, Inc. All Rights Reserved.

#include "PVPDemoEditor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVPDemoEditor, Log, All);

void FPVPDemoEditorModule::StartupModule()
{
	UE_LOG(LogPVPDemoEditor, Log, TEXT("PVPDemoEditor module started"));
}

void FPVPDemoEditorModule::ShutdownModule()
{
	UE_LOG(LogPVPDemoEditor, Log, TEXT("PVPDemoEditor module shut down"));
}

IMPLEMENT_MODULE(FPVPDemoEditorModule, PVPDemoEditor)
