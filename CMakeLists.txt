cmake_minimum_required (VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
project(fucktdesktop C CXX)


if(DEFINED MSVC_CXX_ARCHITECTURE_ID)
    message(STATUS "Using MSVC /MT")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(detours)
add_subdirectory(shim)

add_library(fixdpiawareness OBJECT fixdpiawareness.cpp)
target_compile_features(fixdpiawareness PRIVATE cxx_std_17)
target_link_libraries(fixdpiawareness detours)

add_library(version SHARED "${SHIM_MODULE_DEFINITION}")
target_link_libraries(version fixdpiawareness shim_c)

add_executable(loader loader.cpp)
target_compile_definitions(loader PRIVATE CONSOLE)
target_compile_features(loader PRIVATE cxx_std_20)
target_link_libraries(loader detours)

add_executable(loaderw WIN32 loader.cpp)
target_compile_features(loaderw PRIVATE cxx_std_20)
target_link_libraries(loaderw detours)
