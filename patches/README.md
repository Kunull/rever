# ImGui patches

## imgui-rever-tabs.patch

Applied to the ImGui docking branch (v1.91.8-docking) to:

- **Dock/tab borders**: Only the **active** tab gets a white border (via `TabSelectedOverline`); inactive tab and dock borders use `ImGuiCol_Border` (dark in Rever theme).
- **Active tab**: White background, **black text** (push `ImGuiCol_Text` when drawing the selected tab label).
- **Close button**: Always visible on tabs (remove hover requirement).

**Apply after first configure** (so that `build/_deps/imgui-src` exists):

```bash
cd build/_deps/imgui-src && patch -p0 -i ../../../patches/imgui-rever-tabs.patch
```

Then run `cmake --build build` again. If you do a clean re-configure (e.g. delete `build`), re-apply the patch after the first `cmake ..` so the fetched ImGui is patched before building.
