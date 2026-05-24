cmake_minimum_required(VERSION 3.20)
project(BlackAndWhiteHeadTracking VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Black & White (2001) is 32-bit. Refuse to misconfigure.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "Black & White is a 32-bit game. Configure with -A Win32 (e.g. cmake -B build -A Win32).")
endif()

option(HEADTRACKING_DEBUG_LOGGING "Enable debug file logging" OFF)
option(HEADTRACKING_BUILD_TESTS "Build the host-side unit tests" OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    if(HEADTRACKING_DEBUG_LOGGING)
        add_definitions(-DHEADTRACKING_DEBUG_LOGGING=1)
    endif()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# MinHook (vendored in cameraunlock-core/vendor/minhook).
set(MINHOOK_DIR "${CMAKE_SOURCE_DIR}/cameraunlock-core/vendor/minhook")
if(EXISTS "${MINHOOK_DIR}/CMakeLists.txt")
    add_subdirectory(${MINHOOK_DIR} minhook)
else()
    include(FetchContent)
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4
    )
    FetchContent_MakeAvailable(minhook)
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_DISCOVERY OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/cameraunlock-core/cpp cameraunlock-core)

# glm for matrix math (header-only).
find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# ---------- HeadTracking.dll (injected into runblack.exe) ----------
add_library(HeadTracking SHARED
    src/main.cpp
    src/plugin.cpp
    src/config.cpp
    src/camera_hook.cpp
    src/water_surface.cpp
    src/hotkey_handler.cpp
)

target_include_directories(HeadTracking PRIVATE src)
target_link_libraries(HeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
    glm::glm
    ws2_32
    ddraw
    dxguid
)

set_target_properties(HeadTracking PROPERTIES
    PREFIX ""
    OUTPUT_NAME "HeadTracking"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(HeadTracking PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()

# ---------- bw-headtracking-launcher.exe ----------
add_executable(bw-headtracking-launcher src/loader_main.cpp)
set_target_properties(bw-headtracking-launcher PROPERTIES
    OUTPUT_NAME "bw-headtracking-launcher"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(MSVC)
    target_compile_options(bw-headtracking-launcher PRIVATE /W4 /permissive-)
endif()

# ---------- host-side unit tests (no engine / D3D dependency) ----------
if(HEADTRACKING_BUILD_TESTS)
    add_executable(water_shading_test test/water_shading_test.cpp)
    target_include_directories(water_shading_test PRIVATE src)
    if(MSVC)
        target_compile_options(water_shading_test PRIVATE /W4 /permissive-)
    endif()
    enable_testing()
    add_test(NAME water_shading_test COMMAND water_shading_test)
endif()

message(STATUS "")
message(STATUS "Black & White head tracking ${PROJECT_VERSION} (Win32)")
message(STATUS "  Debug logging: ${HEADTRACKING_DEBUG_LOGGING}")
message(STATUS "")
