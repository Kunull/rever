# Rever

Binary viewer: **ImGui (C++)** frontend with **C++** backend.

## Layout

- **Frontend** (`src/main.cpp`): ImGui + GLFW + OpenGL window; calls the backend.
- **Backend** (`src/backend.cpp`): C++ logic (e.g. `load_file(path)` → raw bytes).

## Build

From the project root:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Or from the project root without `cd`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target rever-imgui
```

Requires: C++17, CMake 3.16+, OpenGL, GLFW (and Qt6 optional for the Qt frontend).

## Run

**ImGui frontend** (recommended):

```bash
./build/rever-imgui.app/Contents/MacOS/rever-imgui
```

Or open `build/rever-imgui.app` in Finder on macOS.

**Qt frontend** (if Qt6 was found at configure time):

```bash
./build/rever.app/Contents/MacOS/rever
```

- **File → Open**: loads a file via the C++ backend (path is currently a placeholder; add a file dialog to choose a path).
- **Help → ImGui Demo**: toggles the ImGui demo window.

## Backend API

- `load_file(path)` — load a file and return `std::vector<uint8_t>` (empty on error).
