cmake_minimum_required(VERSION 3.16)
project(rever VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

# Capstone disassembly engine
FetchContent_Declare(capstone
  GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
  GIT_TAG 5.0.1
)
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "")
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "")
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "")
FetchContent_MakeAvailable(capstone)

# ── ImGui + GLFW version ────────────────────────────────────────────────────
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8-docking
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)

add_executable(rever-imgui MACOSX_BUNDLE
  src/main_imgui.cpp
  src/core/app_state.cpp
  src/core/backend.cpp
  src/viz/gl_helpers.cpp
  src/viz/trigram.cpp
  src/viz/range_selector.cpp
  src/viz/digram.cpp
  src/ui/ui_legends.cpp
  src/ui/ui_panels.cpp
  src/filedialog_mac.mm
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set_target_properties(rever-imgui PROPERTIES
  AUTOMOC OFF
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.rever-imgui.app"
  MACOSX_BUNDLE_BUNDLE_NAME "Rever ImGui"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

target_include_directories(rever-imgui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${capstone_SOURCE_DIR}/include
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

find_library(COCOA_FRAMEWORK Cocoa)
target_link_libraries(rever-imgui PRIVATE glfw OpenGL::GL capstone ${COCOA_FRAMEWORK})
target_compile_definitions(rever-imgui PRIVATE
  GL_SILENCE_DEPRECATION
  REVER_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Copy font into app bundle Resources so macOS sandbox doesn't block access
add_custom_command(TARGET rever-imgui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:rever-imgui>/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/RobotoMono.ttf"
    "$<TARGET_BUNDLE_DIR:rever-imgui>/Contents/Resources/RobotoMono.ttf"
)

