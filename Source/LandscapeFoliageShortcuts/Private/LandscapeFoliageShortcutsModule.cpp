#include "LandscapeFoliageShortcutsModule.h"
#include "Framework/Application/SlateApplication.h"

// Forward declarations of registration functions
namespace LandscapeBrush { void Register(); void Unregister(); }

#define LOCTEXT_NAMESPACE "FLandscapeFoliageShortcutsModule"

void FLandscapeFoliageShortcutsModule::StartupModule()
{
	// Register input processors - module loads PostEngineInit so Slate is ready
	if (FSlateApplication::IsInitialized())
	{
		LandscapeBrush::Register();
	}
}

void FLandscapeFoliageShortcutsModule::ShutdownModule()
{
	// Unregister input processors
	LandscapeBrush::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLandscapeFoliageShortcutsModule, LandscapeFoliageShortcuts)
