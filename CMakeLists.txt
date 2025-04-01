cmake_minimum_required(VERSION 3.28.1)

set(CMAKE_GENERATOR_PLATFORM x64)

project(ergg
  VERSION   "0.0.1"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

include(FetchContent)

FetchContent_Declare(mini
  GIT_REPOSITORY        https://github.com/metayeti/mINI.git
  GIT_TAG               0.9.15
  CONFIGURE_COMMAND     ""
  BUILD_COMMAND         "")

set(SPDLOG_DISABLE_DEFAULT_LOGGER ON)
set(SPDLOG_USE_STD_FORMAT ON)
FetchContent_Declare(spdlog
  GIT_REPOSITORY        https://github.com/gabime/spdlog.git
  GIT_TAG               v1.13.0)

FetchContent_Declare(kiero
  GIT_REPOSITORY        https://github.com/Rebzzel/kiero
  GIT_TAG               1.2.12
  CONFIGURE_COMMAND     ""
  BUILD_COMMAND         ""
  PATCH_COMMAND         git apply ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kiero.patch
  UPDATE_DISCONNECTED   1)

FetchContent_Declare(imgui
  GIT_REPOSITORY        https://github.com/ocornut/imgui.git
  GIT_TAG               v1.91.6
  PATCH_COMMAND         git apply ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui.patch
  UPDATE_DISCONNECTED   1)

FetchContent_Declare(stb
  GIT_REPOSITORY        https://github.com/nothings/stb.git
  GIT_TAG               5c205738c191bcb0abc65c4febfa9bd25ff35234)

FetchContent_Declare(freetype
  GIT_REPOSITORY        https://gitlab.freedesktop.org/freetype/freetype.git
  GIT_TAG               VER-2-13-3)

set(SWSDK_VERSION "1.59")
set(SWSDK_RELEASE "x64")
FetchContent_Declare(steamworks-sdk
  URL                   https://github.com/julianxhokaxhiu/SteamworksSDKCI/releases/download/${SWSDK_VERSION}/SteamworksSDK-v${SWSDK_VERSION}.0_${SWSDK_RELEASE}.zip
  CONFIGURE_COMMAND     ""
  BUILD_COMMAND         "")

FetchContent_Declare(elden-x
  GIT_REPOSITORY        https://github.com/ThomasJClark/elden-x.git
  GIT_TAG               d908c22019288a546e48dd73f56c2596d342c9cb)

add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)

FetchContent_MakeAvailable(
  mini
  spdlog
  kiero
  imgui
  stb
  freetype
  steamworks-sdk
  elden-x)

add_library(mini INTERFACE)
target_include_directories(mini INTERFACE ${mini_SOURCE_DIR}/src)

add_library(imgui
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp)
target_include_directories(imgui PRIVATE ${freetype_SOURCE_DIR}/include)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_library(kiero ${kiero_SOURCE_DIR}/kiero.cpp)
target_include_directories(kiero INTERFACE ${kiero_SOURCE_DIR})

add_library(steamworks-sdk STATIC IMPORTED GLOBAL)
target_include_directories(steamworks-sdk INTERFACE ${steamworks-sdk_SOURCE_DIR}/include)
set_property(TARGET steamworks-sdk APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(steamworks-sdk PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG CXX
        IMPORTED_LOCATION_DEBUG ${steamworks-sdk_SOURCE_DIR}/lib/steam/steam_api64.lib)
 
add_library(${PROJECT_NAME} SHARED
  src/config.cpp
  src/dllmain.cpp
  src/fake_block.cpp
  src/player_list.cpp
  src/renderer/renderer.cpp
  src/renderer/texture.cpp
  src/gui/render_disconnect.cpp
  src/gui/render_block_player.cpp
  src/gui/render_logs.cpp
  src/gui/render_player_list.cpp
  src/gui/render_overlay.cpp
  src/gui/utils.cpp)

target_sources(${PROJECT_NAME} PRIVATE resources/resources.rc)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

add_definitions(-DPROJECT_VERSION="${CMAKE_PROJECT_VERSION}")

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/ergg.ini
  COMMAND_EXPAND_LISTS)
  
target_link_libraries(${PROJECT_NAME} PRIVATE
  mini
  spdlog
  kiero
  imgui
  stb
  freetype
  steamworks-sdk
  elden-x
  d3d12.lib
  dxgi.lib)
