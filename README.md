# Landscape & Foliage Shortcuts

Unreal Engine 5 editor plugin that adds keyboard shortcuts for landscape sculpting/painting and foliage editing.

## What's Included

- **Quick mode switching** — Q for Sculpt, E for Paint. No more clicking tabs.
- **Number key tool/layer selection** — 1-9 selects tools in Sculpt mode (matching left-to-right UI order) or paint layers in Paint mode (respects your display order: Default, Alphabetical, or Custom).
- **Scroll-wheel brush size** — Hold G and scroll to resize the brush in both Landscape and Foliage modes. Multiplicative scaling so it feels natural at any size.
- **Scroll-wheel strength/density** — Hold V and scroll to adjust tool strength (Landscape) or paint density (Foliage) in 10% increments.
- **Foliage presets** — Save/load foliage type selections with Ctrl+1-9 / 1-9. Presets persist in `Config/FoliagePresets.ini` so they survive editor restarts. Presets match by source mesh path so they work across levels.

## Installation

1. Download or clone this repo into your project's `Plugins/` folder
2. Rebuild your project
3. Enable the plugin in Edit > Plugins if not auto-enabled

## Landscape Mode Shortcuts

| Shortcut | Action |
|----------|--------|
| Q | Switch to Sculpt mode |
| E | Switch to Paint mode |
| G + Scroll | Adjust brush size |
| V + Scroll | Adjust tool strength (10% increments) |
| 1-9 | **Sculpt mode:** select tool (Sculpt, Erase, Smooth, Flatten, Ramp, Erosion, Hydro, Noise, Visibility) |
| 1-9 | **Paint mode:** select paint layer (respects display order) |

## Foliage Mode Shortcuts

| Shortcut | Action |
|----------|--------|
| G + Scroll | Adjust brush size |
| V + Scroll | Adjust paint density (10% increments) |
| 1-9 | Load foliage preset |
| Ctrl + 1-9 | Save current foliage selection as preset |
| Alt + 1 | Deselect all foliage types |

Foliage presets are saved to `Config/FoliagePresets.ini` in your project and persist across sessions.

## Compatibility

Developed on UE 5.6. Uses private headers from `LandscapeEditor` and `FoliageEdit` modules, so may need minor adjustments for other engine versions.

## License

MIT
