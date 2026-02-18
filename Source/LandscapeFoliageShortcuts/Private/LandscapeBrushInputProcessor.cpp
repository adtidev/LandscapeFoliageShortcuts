// LandscapeBrushInputProcessor.cpp
// Editor-only input processor for Landscape/Foliage mode shortcuts:
// Landscape mode:
//   - Q: Switch to Sculpt mode
//   - E: Switch to Paint mode
//   - G+Scroll: Brush size adjustment
//   - V+Scroll: Tool strength adjustment (10% increments)
//   - 1-9: Sculpt mode: tool switching; Paint mode: layer selection (grass, dirt, etc.)
// Foliage mode:
//   - G+Scroll: Brush size adjustment
//   - V+Scroll: Paint density adjustment (10% increments)
//   - 1-9: Load foliage preset (replaces selection)
//   - Ctrl+1-9: Save current selection as preset
//   - Alt+1: Deselect all foliage types

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EdMode.h"                    // For FEdMode::GetToolkit()
#include "Toolkits/BaseToolkit.h"      // For FModeToolkit::SetCurrentPalette()
#include "LandscapeEditorObject.h"
#include "LandscapeEdMode.h"           // Private header - for FEdModeLandscape
#include "LandscapeEditTypes.h"        // For ELandscapeToolTargetType
#include "LandscapeInfo.h"             // For ULandscapeInfo::Layers
#include "LandscapeLayerInfoObject.h"  // For ULandscapeLayerInfoObject::bNoWeightBlend
#include "Landscape.h"                 // For ALandscape::TargetDisplayOrderList
#include "FoliageEdMode.h"             // Private header - for FEdModeFoliage
#include "FoliageType.h"
#include "UObject/UObjectIterator.h"   // For TObjectIterator<UFoliageType>
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

class FLandscapeBrushInputProcessor : public IInputProcessor
{
public:
	static TSharedPtr<FLandscapeBrushInputProcessor> Instance;
	static float SharedBrushSize;

	static void Register()
	{
		if (!Instance.IsValid() && FSlateApplication::IsInitialized())
		{
			Instance = MakeShared<FLandscapeBrushInputProcessor>();
			FSlateApplication::Get().RegisterInputPreProcessor(Instance);
			SharedBrushSize = 2048.0f;
		}
	}

	static void Unregister()
	{
		if (Instance.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Instance);
			Instance.Reset();
		}
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		// Track when landscape mode becomes active to sync brush visibility
		bool bLandscapeModeNowActive = GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape);

		if (bLandscapeModeNowActive && !bWasLandscapeModeActive)
		{
			// Just entered landscape mode - check if Ramp tool is selected
			FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
			if (Mode)
			{
				FEdModeLandscape* LandscapeMode = static_cast<FEdModeLandscape*>(Mode);
				if (LandscapeMode->CurrentTool)
				{
					FString CurrentToolName = LandscapeMode->CurrentTool->GetToolName();
					bool bIsRamp = (CurrentToolName == TEXT("Ramp"));
					UpdateBrushVisibilityForTool(bIsRamp);
				}
			}
		}
		else if (!bLandscapeModeNowActive && bWasLandscapeModeActive)
		{
			// Just exited landscape mode - reset our tracking state
			bIsRampToolActive = false;
		}

		bWasLandscapeModeActive = bLandscapeModeNowActive;
	}

	// Check if a level viewport currently has keyboard focus (not a text field or panel)
	bool IsViewportFocused() const
	{
		TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!FocusedWidget.IsValid())
		{
			return false;
		}

		// Walk up the widget tree looking for a viewport widget
		TSharedPtr<SWidget> Current = FocusedWidget;
		while (Current.IsValid())
		{
			FString WidgetType = Current->GetType().ToString();
			if (WidgetType.Contains(TEXT("Viewport")) || WidgetType.Contains(TEXT("SLevelViewport")))
			{
				return true;
			}
			Current = Current->GetParentWidget();
		}

		return false;
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::G)
		{
			bGKeyDown = true;
		}
		if (InKeyEvent.GetKey() == EKeys::V)
		{
			bVKeyDown = true;
		}

		// Number key shortcuts only fire when viewport is focused (not typing in settings fields)
		const bool bViewportHasFocus = IsViewportFocused();

		// Landscape mode shortcuts
		if (bViewportHasFocus && GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape) && !InKeyEvent.IsControlDown() && !InKeyEvent.IsShiftDown())
		{
			// Q/E: Toggle between Sculpt and Paint modes
			if (InKeyEvent.GetKey() == EKeys::Q)
			{
				SwitchLandscapeModePalette(TEXT("ToolMode_Sculpt"));
				// Sculpt mode defaults to Sculpt tool (not Ramp), so restore brush
				UpdateBrushVisibilityForTool(false);
				return true;
			}
			if (InKeyEvent.GetKey() == EKeys::E)
			{
				SwitchLandscapeModePalette(TEXT("ToolMode_Paint"));
				// Paint mode doesn't have Ramp tool, so restore brush
				UpdateBrushVisibilityForTool(false);
				return true;
			}

			// 1-9: In Sculpt mode, switch tools; in Paint mode, select paint layer
			int32 KeyIndex = GetKeyIndex(InKeyEvent.GetKey());
			if (KeyIndex != INDEX_NONE)
			{
				FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
				if (Mode)
				{
					FEdModeLandscape* LandscapeMode = static_cast<FEdModeLandscape*>(Mode);
					if (LandscapeMode->CurrentToolMode)
					{
						FName ModeName = LandscapeMode->CurrentToolMode->ToolModeName;
						if (ModeName == TEXT("ToolMode_Paint"))
						{
							// In Paint mode: select paint layer
							SwitchPaintLayerByIndex(LandscapeMode, KeyIndex);
						}
						else
						{
							// In Sculpt/Manage mode: switch tools
							SwitchLandscapeToolByIndex(KeyIndex);
						}
					}
				}
				return true;
			}
		}

		// Foliage mode preset system
		if (bViewportHasFocus && GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Foliage))
		{
			int32 PresetIndex = GetKeyIndex(InKeyEvent.GetKey());
			if (PresetIndex != INDEX_NONE)
			{
				if (InKeyEvent.IsControlDown())
				{
					// Ctrl+1-9: Save preset
					SaveFoliagePreset(PresetIndex);
					return true;
				}
				else if (InKeyEvent.IsAltDown())
				{
					// Alt+1: Deselect all (only Alt+1, not Alt+2-9)
					if (PresetIndex == 0)
					{
						DeselectAllFoliage();
					}
					return true;
				}
				else if (!InKeyEvent.IsShiftDown())
				{
					// 1-9: Load preset (but not Shift+1-9, those switch editor modes)
					LoadFoliagePreset(PresetIndex);
					return true;
				}
			}
		}

		return false; // Don't consume
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::G)
		{
			bGKeyDown = false;
		}
		if (InKeyEvent.GetKey() == EKeys::V)
		{
			bVKeyDown = false;
		}
		return false; // Don't consume
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		bool bInLandscapeMode = GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape);
		bool bInFoliageMode = GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Foliage);

		float ScrollDelta = InWheelEvent.GetWheelDelta();

		// V+Scroll: Adjust tool strength (Landscape mode) or paint density (Foliage mode)
		if (bVKeyDown)
		{
			if (bInLandscapeMode)
			{
				float StrengthDelta = (ScrollDelta > 0) ? 0.1f : -0.1f;

				for (TObjectIterator<ULandscapeEditorObject> It; It; ++It)
				{
					float NewStrength = FMath::Clamp((*It)->ToolStrength + StrengthDelta, 0.0f, 1.0f);
					(*It)->ToolStrength = NewStrength;
					(*It)->PaintToolStrength = NewStrength;
				}
				return true;
			}

			if (bInFoliageMode)
			{
				// Directly adjust paint density through FEdModeFoliage::UISettings
				FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
				if (Mode)
				{
					FEdModeFoliage* FoliageMode = static_cast<FEdModeFoliage*>(Mode);
					float DensityDelta = (ScrollDelta > 0) ? 0.1f : -0.1f;
					float CurrentDensity = FoliageMode->UISettings.GetPaintDensity();
					float NewDensity = FMath::Clamp(CurrentDensity + DensityDelta, 0.0f, 1.0f);
					FoliageMode->UISettings.SetPaintDensity(NewDensity);
				}
				return true;
			}
		}

		// G+Scroll: Adjust brush size
		if (!bGKeyDown)
		{
			return false;
		}

		if (!bInLandscapeMode && !bInFoliageMode)
		{
			return false;
		}

		// Handle Landscape mode brush size
		if (bInLandscapeMode)
		{
			float SizeMultiplier = (ScrollDelta > 0) ? 1.15f : 0.87f;
			float NewSize = SharedBrushSize * SizeMultiplier;
			NewSize = FMath::Clamp(NewSize, 10.0f, 65536.0f);
			SharedBrushSize = NewSize;

			for (TObjectIterator<ULandscapeEditorObject> It; It; ++It)
			{
				(*It)->BrushRadius = NewSize;
				(*It)->PaintBrushRadius = NewSize;
			}
		}

		// Handle Foliage mode - route through the editor mode's InputKey
		if (bInFoliageMode)
		{
			FEdMode* FoliageMode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
			if (FoliageMode)
			{
				FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
				TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();

				if (ActiveLevelViewport.IsValid())
				{
					TSharedPtr<FEditorViewportClient> ViewportClient = ActiveLevelViewport->GetViewportClient();
					if (ViewportClient.IsValid())
					{
						FViewport* Viewport = ViewportClient->Viewport;
						FKey KeyToPress = (ScrollDelta > 0) ? EKeys::RightBracket : EKeys::LeftBracket;

						int32 NumPresses = FMath::Max(1, FMath::Abs(FMath::RoundToInt(ScrollDelta)));
						for (int32 i = 0; i < NumPresses; i++)
						{
							FoliageMode->InputKey(ViewportClient.Get(), Viewport, KeyToPress, EInputEvent::IE_Pressed);
						}
					}
				}
			}
		}

		return true; // Consume the scroll event
	}

private:
	bool bGKeyDown = false;
	bool bVKeyDown = false;
	bool bIsRampToolActive = false;
	bool bWasLandscapeModeActive = false;

	// Map number keys to key index (0-8)
	int32 GetKeyIndex(FKey Key)
	{
		if (Key == EKeys::One)   return 0;
		if (Key == EKeys::Two)   return 1;
		if (Key == EKeys::Three) return 2;
		if (Key == EKeys::Four)  return 3;
		if (Key == EKeys::Five)  return 4;
		if (Key == EKeys::Six)   return 5;
		if (Key == EKeys::Seven) return 6;
		if (Key == EKeys::Eight) return 7;
		if (Key == EKeys::Nine)  return 8;
		return INDEX_NONE;
	}

	// Get tool name for key in Sculpt mode (matches UI order left-to-right)
	// 1=Sculpt, 2=Erase, 3=Smooth, 4=Flatten, 5=Ramp, 6=Erosion, 7=Hydro, 8=Noise, 9=Visibility
	FName GetSculptToolName(int32 KeyIndex)
	{
		static const FName SculptTools[] = {
			TEXT("Sculpt"),        // 1
			TEXT("Erase"),         // 2
			TEXT("Smooth"),        // 3
			TEXT("Flatten"),       // 4
			TEXT("Ramp"),          // 5
			TEXT("Erosion"),       // 6
			TEXT("HydraErosion"),  // 7 - "Hydro" in UI
			TEXT("Noise"),         // 8
			TEXT("Mask")           // 9 - "Visibility" in UI
		};
		if (KeyIndex >= 0 && KeyIndex < 9)
		{
			return SculptTools[KeyIndex];
		}
		return NAME_None;
	}

	// Get tool name for key in Paint mode (matches UI order)
	// 1=Paint, 2=Smooth, 3=Flatten, 4=Noise, 5=Select
	FName GetPaintToolName(int32 KeyIndex)
	{
		static const FName PaintTools[] = {
			TEXT("Paint"),         // 1
			TEXT("Smooth"),        // 2
			TEXT("Flatten"),       // 3
			TEXT("Noise"),         // 4
			TEXT("Select")         // 5
		};
		if (KeyIndex >= 0 && KeyIndex < 5)
		{
			return PaintTools[KeyIndex];
		}
		return NAME_None;
	}

	// Get tool name for key in Manage mode (matches UI order)
	// 1=NewLandscape, 2=ImportExport, 3=Select, 4=AddComponent, 5=DeleteComponent, 6=MoveToLevel, 7=ResizeLandscape, 8=Splines
	// Note: 9 (BlueprintBrush/Mirror) is excluded - causes crashes
	FName GetManageToolName(int32 KeyIndex)
	{
		static const FName ManageTools[] = {
			TEXT("NewLandscape"),      // 1
			TEXT("ImportExport"),      // 2
			TEXT("Select"),            // 3
			TEXT("AddComponent"),      // 4
			TEXT("DeleteComponent"),   // 5
			TEXT("MoveToLevel"),       // 6
			TEXT("ResizeLandscape"),   // 7
			TEXT("Splines")            // 8
		};
		if (KeyIndex >= 0 && KeyIndex < 8)
		{
			return ManageTools[KeyIndex];
		}
		return NAME_None;
	}

	void DeselectAllFoliage()
	{
		// Iterate all loaded foliage types
		for (TObjectIterator<UFoliageType> It; It; ++It)
		{
			UFoliageType* FoliageType = *It;
			if (FoliageType && FoliageType->IsSelected)
			{
				FoliageType->Modify(false);
				FoliageType->IsSelected = false;
			}
		}
	}

	// Get the persistent config file path (in project Config folder, not Saved)
	static FString GetFoliagePresetsConfigPath()
	{
		return FPaths::ProjectConfigDir() / TEXT("FoliagePresets.ini");
	}

	void SaveFoliagePreset(int32 PresetIndex)
	{
		// Collect source asset paths of selected foliage types
		// Using GetSource() returns the underlying mesh/actor, which has a stable asset path
		// (unlike the foliage type instance path which is map-specific and can change)
		TArray<FString> SelectedPaths;
		for (TObjectIterator<UFoliageType> It; It; ++It)
		{
			UFoliageType* FoliageType = *It;
			if (FoliageType && FoliageType->IsSelected)
			{
				UObject* Source = FoliageType->GetSource();
				if (Source)
				{
					FString SourcePath = Source->GetPathName();
					// Avoid duplicates (multiple foliage types can reference the same mesh)
					SelectedPaths.AddUnique(SourcePath);
				}
			}
		}

		FString ConfigPath = GetFoliagePresetsConfigPath();
		FString SectionName = TEXT("FoliagePresets");
		FString KeyName = FString::Printf(TEXT("Preset%d"), PresetIndex);
		FString Value = FString::Join(SelectedPaths, TEXT("|"));

		UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: Saving foliage preset %d to %s"), PresetIndex, *ConfigPath);
		UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: %d foliage types selected"), SelectedPaths.Num());

		// Load existing config, update, and save
		FConfigFile ConfigFile;
		ConfigFile.Read(ConfigPath);
		ConfigFile.SetString(*SectionName, *KeyName, *Value);

		bool bWriteSuccess = ConfigFile.Write(ConfigPath);
		if (!bWriteSuccess)
		{
			// Fallback: write manually using FFileHelper
			UE_LOG(LogTemp, Warning, TEXT("LandscapeFoliageShortcuts: FConfigFile::Write failed, trying FFileHelper"));

			FString FileContents;
			ConfigFile.WriteToString(FileContents);
			bWriteSuccess = FFileHelper::SaveStringToFile(FileContents, *ConfigPath);
		}

		if (bWriteSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: Preset %d saved successfully"), PresetIndex);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("LandscapeFoliageShortcuts: FAILED to save preset %d to %s"), PresetIndex, *ConfigPath);
		}
	}

	void LoadFoliagePreset(int32 PresetIndex)
	{
		FString ConfigPath = GetFoliagePresetsConfigPath();
		FString SectionName = TEXT("FoliagePresets");
		FString KeyName = FString::Printf(TEXT("Preset%d"), PresetIndex);
		FString Value;

		UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: Loading foliage preset %d from %s"), PresetIndex, *ConfigPath);

		// Check if file exists
		if (!FPaths::FileExists(ConfigPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("LandscapeFoliageShortcuts: Preset file does not exist: %s"), *ConfigPath);
			DeselectAllFoliage();
			return;
		}

		FConfigFile ConfigFile;
		ConfigFile.Read(ConfigPath);
		if (!ConfigFile.GetString(*SectionName, *KeyName, Value))
		{
			UE_LOG(LogTemp, Warning, TEXT("LandscapeFoliageShortcuts: Preset %d not found in file"), PresetIndex);
			DeselectAllFoliage();
			return;
		}

		// Parse the saved source asset paths
		TArray<FString> SavedPaths;
		Value.ParseIntoArray(SavedPaths, TEXT("|"), true);
		TSet<FString> SavedPathSet(SavedPaths);

		UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: Preset %d contains %d source assets"), PresetIndex, SavedPaths.Num());

		// Update selection state for all loaded foliage types by matching their source asset
		int32 MatchCount = 0;
		for (TObjectIterator<UFoliageType> It; It; ++It)
		{
			UFoliageType* FoliageType = *It;
			if (FoliageType)
			{
				bool bShouldBeSelected = false;

				// Match by source asset path (mesh or actor class)
				UObject* Source = FoliageType->GetSource();
				if (Source)
				{
					FString SourcePath = Source->GetPathName();
					bShouldBeSelected = SavedPathSet.Contains(SourcePath);
				}

				if (bShouldBeSelected)
				{
					MatchCount++;
				}

				if (FoliageType->IsSelected != bShouldBeSelected)
				{
					FoliageType->Modify(false);
					FoliageType->IsSelected = bShouldBeSelected;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("LandscapeFoliageShortcuts: Matched %d/%d source assets"), MatchCount, SavedPaths.Num());
	}

	// Hide or show the landscape brush based on whether Ramp tool is active
	// Ramp tool has its own gizmo and shouldn't show the brush circle
	void UpdateBrushVisibilityForTool(bool bIsRamp)
	{
		if (bIsRamp == bIsRampToolActive)
		{
			return; // No change needed
		}

		bIsRampToolActive = bIsRamp;

		for (TObjectIterator<ULandscapeEditorObject> It; It; ++It)
		{
			if (bIsRamp)
			{
				// Hide brush by setting radius to 0
				(*It)->BrushRadius = 0.0f;
				(*It)->PaintBrushRadius = 0.0f;
			}
			else
			{
				// Restore brush size from our cached value
				(*It)->BrushRadius = SharedBrushSize;
				(*It)->PaintBrushRadius = SharedBrushSize;
			}
		}
	}

	// Switch to tool at KeyIndex (0-8) by directly setting internal pointers
	// Uses hardcoded tool names to match UI order (ValidTools array order doesn't match UI)
	void SwitchLandscapeToolDirect(int32 TargetKeyIndex)
	{
		FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
		if (!Mode)
		{
			return;
		}

		FEdModeLandscape* LandscapeMode = static_cast<FEdModeLandscape*>(Mode);
		if (!LandscapeMode->CurrentToolMode || !LandscapeMode->CurrentTool)
		{
			return;
		}

		// Get target tool name from hardcoded UI-order list (not ValidTools which has different order)
		FName TargetToolName = GetSculptToolName(TargetKeyIndex);
		if (TargetToolName == NAME_None)
		{
			return;
		}

		// Verify the tool is valid for current mode
		const TArray<FName>& ValidTools = LandscapeMode->CurrentToolMode->ValidTools;
		if (!ValidTools.Contains(TargetToolName))
		{
			return; // Tool not valid for current mode
		}

		// Find the tool index in LandscapeTools array
		int32 NewToolIndex = INDEX_NONE;
		FString TargetToolString = TargetToolName.ToString();
		for (int32 i = 0; i < LandscapeMode->LandscapeTools.Num(); i++)
		{
			if (LandscapeMode->LandscapeTools[i].IsValid())
			{
				FString ToolName = LandscapeMode->LandscapeTools[i]->GetToolName();
				if (TargetToolString == ToolName)
				{
					NewToolIndex = i;
					break;
				}
			}
		}

		if (NewToolIndex == INDEX_NONE || NewToolIndex == LandscapeMode->CurrentToolIndex)
		{
			return; // Tool not found or already selected
		}

		// Update pointers directly
		LandscapeMode->CurrentToolIndex = NewToolIndex;
		LandscapeMode->CurrentTool = LandscapeMode->LandscapeTools[NewToolIndex].Get();
		LandscapeMode->CurrentToolMode->CurrentToolName = TargetToolName;

		// Update brush visibility - hide for Ramp tool, show for others
		UpdateBrushVisibilityForTool(TargetToolName == TEXT("Ramp"));
	}

	// Get tool name for current mode by key index
	FName GetToolNameForCurrentMode(FEdModeLandscape* LandscapeMode, int32 KeyIndex)
	{
		if (!LandscapeMode || !LandscapeMode->CurrentToolMode)
		{
			return NAME_None;
		}

		FName ModeName = LandscapeMode->CurrentToolMode->ToolModeName;

		if (ModeName == TEXT("ToolMode_Sculpt"))
		{
			return GetSculptToolName(KeyIndex);
		}
		else if (ModeName == TEXT("ToolMode_Paint"))
		{
			return GetPaintToolName(KeyIndex);
		}
		// Manage mode - no shortcuts (rarely used, key 9 crashes)

		return NAME_None;
	}

	// Switch landscape mode using the toolkit's SetCurrentPalette (exported API)
	void SwitchLandscapeModePalette(FName PaletteName)
	{
		FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
		if (!Mode)
		{
			return;
		}

		// Get the toolkit from the mode
		TSharedPtr<FModeToolkit> Toolkit = Mode->GetToolkit();
		if (!Toolkit.IsValid())
		{
			return;
		}

		// Switch palette - this calls OnToolPaletteChanged which calls OnChangeMode
		Toolkit->SetCurrentPalette(PaletteName);
	}

	void SwitchLandscapeToolByName(FName TargetToolName)
	{
		if (TargetToolName == NAME_None)
		{
			return;
		}

		FEdMode* Mode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
		if (!Mode)
		{
			return;
		}

		FEdModeLandscape* LandscapeMode = static_cast<FEdModeLandscape*>(Mode);
		if (!LandscapeMode->CurrentTool || !LandscapeMode->CurrentToolMode)
		{
			return;
		}

		// Find the actual tool index in LandscapeTools array
		int32 NewToolIndex = INDEX_NONE;
		FString TargetToolString = TargetToolName.ToString();
		for (int32 i = 0; i < LandscapeMode->LandscapeTools.Num(); i++)
		{
			if (LandscapeMode->LandscapeTools[i].IsValid())
			{
				FString ToolName = LandscapeMode->LandscapeTools[i]->GetToolName();
				if (TargetToolString == ToolName)
				{
					NewToolIndex = i;
					break;
				}
			}
		}

		if (NewToolIndex == INDEX_NONE || NewToolIndex == LandscapeMode->CurrentToolIndex)
		{
			return;
		}

		// Update pointers
		LandscapeMode->CurrentToolIndex = NewToolIndex;
		LandscapeMode->CurrentTool = LandscapeMode->LandscapeTools[NewToolIndex].Get();
		LandscapeMode->CurrentToolMode->CurrentToolName = TargetToolName;
	}

	void SwitchLandscapeToolByIndex(int32 KeyIndex)
	{
		// Use direct tool setting with mouse move to trigger brush update
		SwitchLandscapeToolDirect(KeyIndex);
	}

	// Select paint layer by index (0-8 for keys 1-9)
	// Respects the display order set in the editor (Default, Alphabetical, or Custom/UserSpecific)
	void SwitchPaintLayerByIndex(FEdModeLandscape* LandscapeMode, int32 KeyIndex)
	{
		if (!LandscapeMode)
		{
			return;
		}

		// Get landscape info from current tool target
		ULandscapeInfo* LandscapeInfo = LandscapeMode->CurrentToolTarget.LandscapeInfo.Get();
		if (!LandscapeInfo)
		{
			return;
		}

		// Get layers from ULandscapeInfo
		const TArray<FLandscapeInfoLayerSettings>& Layers = LandscapeInfo->Layers;
		if (Layers.Num() == 0)
		{
			return;
		}

		// Build ordered list of layer names based on display mode
		TArray<FName> OrderedLayerNames;

		// Helper lambda to check if a layer is valid and should be included
		// Skip layers with None/empty names, no LayerInfoObj, or bNoWeightBlend (base layers)
		auto IsValidPaintLayer = [](const FLandscapeInfoLayerSettings& LayerSettings) -> bool
		{
			if (LayerSettings.LayerInfoObj == nullptr)
			{
				return false;
			}
			if (LayerSettings.LayerName.IsNone() || LayerSettings.LayerName == NAME_None)
			{
				return false;
			}
			// Skip "no weight blend" layers - these are base/fill layers that shouldn't be directly painted
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
			if (LayerSettings.LayerInfoObj->bNoWeightBlend)
			{
				return false;
			}
#endif
			return true;
		};

		// Get the landscape actor for display order settings
		ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
		if (Landscape && Landscape->TargetDisplayOrder == ELandscapeLayerDisplayMode::UserSpecific)
		{
			// Custom order - use TargetDisplayOrderList
			// Filter to only include layers that actually exist in Layers array
			for (const FName& LayerName : Landscape->TargetDisplayOrderList)
			{
				if (LayerName.IsNone() || LayerName == NAME_None)
				{
					continue;
				}
				for (const FLandscapeInfoLayerSettings& LayerSettings : Layers)
				{
					if (LayerSettings.LayerName == LayerName && IsValidPaintLayer(LayerSettings))
					{
						OrderedLayerNames.Add(LayerName);
						break;
					}
				}
			}
		}
		else if (Landscape && Landscape->TargetDisplayOrder == ELandscapeLayerDisplayMode::Alphabetical)
		{
			// Alphabetical order
			for (const FLandscapeInfoLayerSettings& LayerSettings : Layers)
			{
				if (IsValidPaintLayer(LayerSettings))
				{
					OrderedLayerNames.Add(LayerSettings.LayerName);
				}
			}
			OrderedLayerNames.Sort([](const FName& A, const FName& B)
			{
				return A.Compare(B) < 0;
			});
		}
		else
		{
			// Default order - use Layers array order (material order)
			for (const FLandscapeInfoLayerSettings& LayerSettings : Layers)
			{
				if (IsValidPaintLayer(LayerSettings))
				{
					OrderedLayerNames.Add(LayerSettings.LayerName);
				}
			}
		}

		// Check if the requested index is valid
		if (KeyIndex < 0 || KeyIndex >= OrderedLayerNames.Num())
		{
			return; // No layer at this index
		}

		// Get the target layer name
		FName TargetLayerName = OrderedLayerNames[KeyIndex];

		// Find the layer info object
		ULandscapeLayerInfoObject* LayerInfoObj = nullptr;
		for (const FLandscapeInfoLayerSettings& LayerSettings : Layers)
		{
			if (LayerSettings.LayerName == TargetLayerName)
			{
				LayerInfoObj = LayerSettings.LayerInfoObj;
				break;
			}
		}

		if (!LayerInfoObj)
		{
			return;
		}

		// Set the target layer (replicating what SetCurrentTargetLayer does)
		if (LandscapeMode->CurrentToolMode)
		{
			LandscapeMode->CurrentToolMode->CurrentTargetLayerName = TargetLayerName;
		}
		LandscapeMode->CurrentToolTarget.TargetType = ELandscapeToolTargetType::Weightmap;
		LandscapeMode->CurrentToolTarget.LayerName = TargetLayerName;
		LandscapeMode->CurrentToolTarget.LayerInfo = LayerInfoObj;
	}
};

TSharedPtr<FLandscapeBrushInputProcessor> FLandscapeBrushInputProcessor::Instance;
float FLandscapeBrushInputProcessor::SharedBrushSize = 2048.0f;

// Namespace for module registration
namespace LandscapeBrush
{
	void Register() { FLandscapeBrushInputProcessor::Register(); }
	void Unregister() { FLandscapeBrushInputProcessor::Unregister(); }
}
