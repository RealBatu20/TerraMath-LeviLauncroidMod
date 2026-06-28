# TerraMath-LeviLauncroidMod
# -----------------------------------------------------------------------------
# Two build modes:
#   * Android (CMAKE_SYSTEM_NAME=Android, via NDK toolchain): builds the mod
#     -> libterramath.so (arm64-v8a). Requires the Levi Launchroid / Gloss
#     preloader SDK and pulls ImGui via FetchContent.
#   * Host (default desktop compiler): builds `terramath_tests`, the portable
#     formula-engine unit tests (no NDK / SDK needed).
# -----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)
project(terramath LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable engine sources (compile everywhere).
set(TM_CORE_SOURCES
    src/math/MathFunctions.cpp
    src/math/Noise.cpp
    src/math/Expr.cpp
    src/terrain/FormulaEngine.cpp
    src/config/Config.cpp
)

# ---------------------------------------------------------------------------
if(ANDROID)
    # --- Levi Launchroid / Gloss preloader SDK -----------------------------
    # Provide the include dir that contains pl/Gloss.h, pl/Signature.h, pl/Logger.h
    # and the Gloss link library. Obtain these from the official Levi Launchroid
    # native-mod template (see docs/BUILD.md).
    set(PRELOADER_INCLUDE "" CACHE PATH "Dir containing pl/Gloss.h, pl/Signature.h")
    set(PRELOADER_LIB     "" CACHE FILEPATH "Gloss/preloader link library (lib*.a/.so)")

    if(NOT PRELOADER_INCLUDE OR NOT EXISTS "${PRELOADER_INCLUDE}/pl/Gloss.h")
        message(FATAL_ERROR
            "PRELOADER_INCLUDE must point to the Levi Launchroid SDK include dir "
            "(containing pl/Gloss.h). See docs/BUILD.md. "
            "Example: -DPRELOADER_INCLUDE=/path/to/preloader-android/include")
    endif()

    # --- ImGui (render UI) -------------------------------------------------
    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5)
    FetchContent_MakeAvailable(imgui)

    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    add_library(terramath SHARED
        ${TM_CORE_SOURCES}
        ${IMGUI_SOURCES}
        src/Main.cpp
        src/hook/EglHook.cpp
        src/hook/InputHook.cpp
        src/hook/TerrainHook.cpp
        src/ui/ImGuiManager.cpp
        src/ui/ConfigUI.cpp
    )

    target_include_directories(terramath PRIVATE
        ${PRELOADER_INCLUDE}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        src
    )

    target_compile_definitions(terramath PRIVATE IMGUI_IMPL_OPENGL_ES3)

    target_compile_options(terramath PRIVATE
        -O2 -fvisibility=hidden -ffunction-sections -fdata-sections
        -Wall -Wextra -Wno-unused-parameter)

    target_link_options(terramath PRIVATE
        -Wl,--gc-sections
        $<$<CONFIG:Release>:-Wl,--strip-all>)

    find_library(EGL_LIB EGL)
    find_library(GLES3_LIB GLESv3)
    find_library(ANDROID_LIB android)
    find_library(LOG_LIB log)

    target_link_libraries(terramath PRIVATE
        ${EGL_LIB} ${GLES3_LIB} ${ANDROID_LIB} ${LOG_LIB} dl)

    if(PRELOADER_LIB)
        target_link_libraries(terramath PRIVATE ${PRELOADER_LIB})
    endif()

    # Output exactly lib<mod_name>.so
    set_target_properties(terramath PROPERTIES OUTPUT_NAME "terramath")

else()
    # --- Host test build ---------------------------------------------------
    enable_testing()
    add_executable(terramath_tests
        ${TM_CORE_SOURCES}
        tests/test_formula.cpp)
    target_include_directories(terramath_tests PRIVATE src)
    target_compile_options(terramath_tests PRIVATE -O2 -Wall -Wextra)
    add_test(NAME formula_engine COMMAND terramath_tests)
endif()
