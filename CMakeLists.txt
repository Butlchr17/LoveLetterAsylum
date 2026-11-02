cmake_minimum_required(VERSION 3.25)

project(LoveLetterAsylum LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    VulkanHeaders
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.341.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(VulkanHeaders)

find_package(Threads REQUIRED)

find_library(VULKAN_LOADER vulkan
    PATHS /usr/lib/x86_64-linux-gnu
    NAMES vulkan libvulkan.so.1
    NO_DEFAULT_PATH)
if(NOT VULKAN_LOADER)
    set(VULKAN_LOADER /usr/lib/x86_64-linux-gnu/libvulkan.so.1)
endif()

add_library(lla_contracts INTERFACE)
target_include_directories(lla_contracts INTERFACE src/contracts/include)

add_library(lla_ui STATIC
    modules/ui/src/text.cpp)
target_include_directories(lla_ui PUBLIC modules/ui/include)
target_link_libraries(lla_ui PUBLIC lla_contracts)

add_library(lla_saves STATIC
    modules/saves/src/save_slots.cpp)
target_include_directories(lla_saves PUBLIC modules/saves/include)
target_link_libraries(lla_saves PUBLIC lla_contracts)
target_compile_options(lla_saves PRIVATE -Wall -Wextra)

add_library(lla_frontend STATIC
    modules/frontend/src/main_menu.cpp)
target_include_directories(lla_frontend PUBLIC modules/frontend/include)
target_link_libraries(lla_frontend PUBLIC lla_ui)

add_library(lla_intro STATIC
    modules/intro/src/intro.cpp)
target_include_directories(lla_intro PUBLIC modules/intro/include)
target_link_libraries(lla_intro PUBLIC lla_ui)
target_compile_options(lla_intro PRIVATE -Wall -Wextra)

add_library(lla_tutorial STATIC
    modules/tutorial/src/tutorial.cpp)
target_include_directories(lla_tutorial PUBLIC modules/tutorial/include)
target_link_libraries(lla_tutorial PUBLIC lla_ui)
target_compile_options(lla_tutorial PRIVATE -Wall -Wextra)

add_library(lla_settings STATIC
    modules/settings/src/device_settings.cpp
    modules/settings/src/settings_menu.cpp)
target_include_directories(lla_settings PUBLIC modules/settings/include)
target_link_libraries(lla_settings PUBLIC lla_ui)
target_compile_options(lla_settings PRIVATE -Wall -Wextra)

add_library(lla_vulkan STATIC
    src/platform/vulkan/xcb_window.cpp
    src/platform/vulkan/renderer.cpp
    src/platform/vulkan/host.cpp)
target_include_directories(lla_vulkan PUBLIC src/platform/vulkan/include)
target_include_directories(lla_vulkan PRIVATE src/platform/vulkan)
target_link_libraries(lla_vulkan
    PUBLIC lla_contracts
    PRIVATE Vulkan::Headers ${VULKAN_LOADER} ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_options(lla_vulkan PRIVATE -Wall -Wextra)

add_executable(save_slots_test tests/save_slots_test.cpp)
target_link_libraries(save_slots_test PRIVATE lla_saves)
target_compile_options(save_slots_test PRIVATE -Wall -Wextra)

add_executable(walk_test tests/walk_test.cpp)
target_link_libraries(walk_test PRIVATE lla_tutorial)
target_compile_options(walk_test PRIVATE -Wall -Wextra)

add_executable(settings_test tests/settings_test.cpp)
target_link_libraries(settings_test PRIVATE lla_settings)
target_compile_options(settings_test PRIVATE -Wall -Wextra)

add_executable(text_fit_test tests/text_fit_test.cpp)
target_link_libraries(text_fit_test PRIVATE lla_ui)
target_compile_options(text_fit_test PRIVATE -Wall -Wextra)

add_executable(loveletter src/host/main.cpp)
target_link_libraries(loveletter PRIVATE lla_frontend lla_intro lla_tutorial lla_saves lla_settings lla_vulkan)
target_compile_options(loveletter PRIVATE -Wall -Wextra)
target_compile_options(lla_ui PRIVATE -Wall -Wextra)
target_compile_options(lla_frontend PRIVATE -Wall -Wextra)
