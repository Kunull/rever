# How ImHex Uses the Native macOS Menu Bar

ImHex shows **File**, **View**, etc. in the **native macOS menu bar** (top of the screen) instead of an in-window ImGui menu bar. Here is how they do it.

## Overview

1. **Cocoa/AppKit** – They use the application’s main menu: `NSApp.mainMenu`.
2. **Mirror the ImGui menu in Cocoa** – Each frame they build/update the same structure (File, Edit, View, …) using `NSMenu` / `NSMenuItem`.
3. **No ImGui menu bar when native is on** – When the native menu bar is enabled, they do **not** call `ImGui::BeginMainMenuBar()` (so no menu bar is drawn inside the window). They still call the same logical flow: “begin main menu bar” → “begin menu File” → “menu item Open…” → … but those calls drive **Cocoa** instead of ImGui.
4. **Clicks** – Menu item actions are handled by an Objective-C handler that sets a “selected tag”. The C++ menu code then checks that tag and returns “this item was clicked” so the app runs the same logic as for ImGui menu items.

## Key Files in ImHex

| File | Role |
|------|------|
| `lib/libimhex/source/helpers/macos_menu.m` | Objective-C: builds `NSApp.mainMenu`, adds `NSMenu`/`NSMenuItem`, handles clicks via `MenuItemHandler` and tag IDs. |
| `plugins/ui/source/ui/menu_items.cpp` | C++: `enableNativeMenuBar(true)`, `beginMainMenuBar()` → calls `macosBeginMainMenuBar()` on macOS when native is on; `BeginMenu`/`MenuItem`/… call the `macos*` C functions. |
| `plugins/ui/source/ui/window_decoration.cpp` | Calls `menu::enableNativeMenuBar(s_useNativeMenuBar)` (setting defaults to true) then `menu::beginMainMenuBar()` and the rest of the menu tree. |

## How the native menu is built (`macos_menu.m`)

1. **Init**  
   `macosMenuBarInit()` stores `NSApp.mainMenu` in a stack so the code can add items to it.

2. **Clear**  
   `macosClearMenu()` removes all main menu items except the first two (keeps the standard Application menu).

3. **Main menu bar**  
   `macosBeginMainMenuBar()` returns `true` and does not draw anything in ImGui. The OS already shows the native bar.

4. **Menus (File, View, …)**  
   `macosBeginMenu(label, …)` creates or finds an `NSMenu` with that label and adds it as a submenu to the current menu (main menu or a submenu). A stack tracks the “current” menu for nesting.

5. **Items (Open…, Theme, …)**  
   `macosMenuItem(label, icon, keyEquivalent, selected, enabled)` creates an `NSMenuItem` with:
   - `action = @selector(OnClick:)` and `target = s_menuItemHandler`
   - a unique `tag` (incremented each item)
   - key equivalent (e.g. Cmd+O) for shortcuts

6. **Clicks**  
   When the user clicks an item, Cocoa calls `MenuItemHandler::OnClick:`. The handler sets `s_selectedTag = menu_item.tag`. When the C++ code later calls `macosMenuItem(…)` for the same logical item, it sees `[menuItem tag] == s_selectedTag`, clears the tag, and returns `true` so the app executes the same action as for an ImGui menu item.

7. **Separators**  
   `macosSeparator()` adds `[NSMenuItem separatorItem]` to the current menu.

## Enabling it in ImHex

- **Setting** – `hex.builtin.setting.interface.use_native_menu_bar` (default **true**), used in `window_decoration.cpp`.
- **Flow** – Each frame they call `menu::enableNativeMenuBar(s_useNativeMenuBar)` then run the same menu structure (beginMainMenuBar → BeginMenu("File") → MenuItem("Open…") → …). On macOS with native enabled, those calls go to the `macos*` functions and update/use `NSApp.mainMenu` instead of drawing an ImGui menu bar.

## What you’d need for Rever

To get the native macOS menu bar in Rever you would:

1. **Add a small Cocoa/ObjC layer** (e.g. `macos_menu.m` or `.mm`) that:
   - Takes `NSApp.mainMenu` and clears/rebuilds it (or updates it) each frame.
   - Creates `NSMenu` / `NSMenuItem` for File, Edit, View, and your items (Open…, Theme, Trigram settings…, etc.).
   - Assigns each item a unique tag and an action that sets “selected tag” when clicked.
   - Maps key equivalents (Cmd+O, Cmd+Q, Ctrl+G, etc.).

2. **In your main loop (e.g. `main_imgui.cpp`)** on macOS:
   - Optionally skip `ImGui::BeginMainMenuBar()` / `ImGui::EndMainMenuBar()` when using the native bar.
   - Call into the Cocoa layer to build/update the native menu (same structure as your current ImGui menu).
   - When the Cocoa handler sets “item X was clicked”, run the same code you currently run for that ImGui menu item (e.g. open file, show Trigram settings, toggle theme).

3. **Build** – Compile the `.m`/`.mm` file with the rest of the app and link Cocoa/Foundation/AppKit (Rever already links Cocoa for the window).

ImHex’s `macos_menu.m` and `menu_items.cpp` are the reference implementation for this pattern; the doc above summarizes how they connect and how you could do the same for Rever.
