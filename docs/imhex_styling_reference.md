# ImHex styling reference

Pulled from [ImHex](https://github.com/WerWolv/ImHex) for reference. ImHex uses JSON themes with `colors` (per handler: imgui, imhex, implot, imnodes, text-editor) and `styles` (imgui, imhex, implot, imnodes).

## Where ImHex defines styling

- **Theme format**: JSON with `name`, `base` (optional), `colors`, `styles`, `image_theme`.
- **Built-in themes**: `plugins/builtin/romfs/themes/` — `dark.json`, `light.json`, `classic.json`.
- **Theme/color/style registration**: `plugins/builtin/source/content/themes.cpp` — `registerThemeHandlers()` (ImGui/ImPlot/ImNodes/ImHex/text-editor color maps), `registerStyleHandlers()` (ImGui/ImPlot/ImNodes/ImHex style maps).
- **Theme manager**: `lib/libimhex/source/api/theme_manager.cpp` — loads JSON, applies colors and styles via handlers.

## ImGui style values (ImHex Dark theme)

From `plugins/builtin/romfs/themes/dark.json` → `styles.imgui`:

| Key | Value | Notes |
|-----|--------|--------|
| alpha | 1 | |
| disabled-alpha | 0.6 | |
| window-padding | [8, 8] | |
| window-rounding | 0 | |
| window-border-size | 1 | |
| window-border-hover-padding | 4 | |
| window-min-size | [32, 32] | |
| window-title-align | [0, 0.5] | |
| child-rounding | 0 | |
| child-border-size | 1 | |
| popup-rounding | 0 | |
| popup-border-size | 1 | |
| frame-padding | [4, 3] | |
| frame-rounding | 0 | |
| frame-border-size | 0 | |
| item-spacing | [8, 4] | |
| item-inner-spacing | [4, 4] | |
| cell-padding | [4, 2] | |
| touch-extra-padding | [0, 0] | |
| indent-spacing | 21 | |
| columns-min-spacing | 6 | |
| scrollbar-size | 14 | |
| scrollbar-rounding | 9 | |
| grab-min-size | 12 | |
| grab-rounding | 0 | |
| tab-rounding | 5 | |
| tab-border-size | 0 | |
| tab-bar-border-size | 1 | |
| tab-bar-overline-size | 1 | |
| button-text-align | [0.5, 0.5] | |
| selectable-text-align | [0, 0] | |
| separator-text-border-size | 3 | |
| separator-text-align | [0, 0.5] | |
| separator-text-padding | [20, 3] | |
| display-window-padding | [19, 19] | |
| display-safe-area-padding | [3, 3] | |
| docking-separator-size | 2 | |

So ImHex Dark uses **no rounding** on windows, child, popup, frame, grab; **tab-rounding 5** and **scrollbar-rounding 9**; padding/spacing **8,8 / 4,3 / 8,4 / 4,4 / 4,2** as in Rever’s `Design::Style`.

## ImGui color keys (ImHex → ImGuiCol)

ImHex theme JSON uses kebab-case keys under `colors.imgui`. Mapping is in `themes.cpp` `ImGuiColorMap`, e.g.:

- `text` → ImGuiCol_Text  
- `text-disabled` → ImGuiCol_TextDisabled  
- `window-background` → ImGuiCol_WindowBg  
- `frame-background` → ImGuiCol_FrameBg  
- `button`, `button-hovered`, `button-active`  
- `tab`, `tab-hovered`, `tab-active`, `tab-active-overline`, `tab-unfocused`, `tab-unfocused-active`  
- Plus all other ImGuiCol_* (border, scrollbar-grab, header, etc.).  
Colors are stored as `#AARRGGBB` hex; `*` prefix means “accent tint” in ImHex.

## Rever alignment

Rever’s `src/ui/design.hpp` and `setupTheme()` (built-in Rever theme) already match these ImHex-style values:

- `Design::Style`: WindowPadding 8,8; FramePadding 4,3; ItemSpacing 8,4; CellPadding 4,2.  
- Built-in theme: WindowRounding 0, FrameRounding 0, no rounding on windows/frames; same padding/spacing.

So Rever is already ImHex-aligned for layout and style numbers; themes from `themes.toml` (e.g. ImThemes) override colors and optional rounding.
