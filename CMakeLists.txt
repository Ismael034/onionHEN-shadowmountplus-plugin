cmake_minimum_required(VERSION 3.20)

project(OnionHENShadowMountPlusPlugin VERSION 1.0.0 LANGUAGES C)

set(ONION_PLUGIN_TARGET shadowmountplus)
set(ONION_PLUGIN_ID SMPL00001)
set(ONION_PLUGIN_VERSION 1.00)
set(ONION_PLUGIN_NAME "ShadowMount+")
set(SHADOWMOUNT_UPSTREAM_VERSION "1.6beta16-onion")

set(ONIONHEN_PLUGIN_SDK_SOURCE "" CACHE PATH
    "Use a local onionHEN-plugin-sdk checkout instead of downloading it")
set(ONIONHEN_PLUGIN_SDK_GIT_REPOSITORY
    "https://github.com/OnionBuddies/onionHEN-plugin-sdk.git" CACHE STRING
    "OnionHEN Plugin SDK repository")
set(ONIONHEN_PLUGIN_SDK_GIT_TAG
    "0cf4346536705fb271ba0eb2226d97ebcb23559c" CACHE STRING
    "Pinned OnionHEN Plugin SDK revision")

if(NOT ONIONHEN_PLUGIN_SDK_SOURCE AND
   DEFINED ENV{ONIONHEN_PLUGIN_SDK_SOURCE})
    set(ONIONHEN_PLUGIN_SDK_SOURCE "$ENV{ONIONHEN_PLUGIN_SDK_SOURCE}")
endif()

if(ONIONHEN_PLUGIN_SDK_SOURCE)
    if(NOT EXISTS "${ONIONHEN_PLUGIN_SDK_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR
            "ONIONHEN_PLUGIN_SDK_SOURCE is not an SDK checkout: "
            "${ONIONHEN_PLUGIN_SDK_SOURCE}")
    endif()
    add_subdirectory(
        "${ONIONHEN_PLUGIN_SDK_SOURCE}"
        "${CMAKE_BINARY_DIR}/_deps/onionhen-plugin-sdk-build"
        EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(onionhen_plugin_sdk
        GIT_REPOSITORY "${ONIONHEN_PLUGIN_SDK_GIT_REPOSITORY}"
        GIT_TAG "${ONIONHEN_PLUGIN_SDK_GIT_TAG}"
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(onionhen_plugin_sdk)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/plugin_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/plugin_config.h"
    @ONLY)

set(SHADOWMOUNT_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ShadowMountPlus")
set(SHADOWMOUNT_SOURCES
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_appdb.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_config_mount.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_fakelib.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_filesystem.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_gameinfo.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_game_cache.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_game_lifecycle.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_image.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_image_cache.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_install.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_install_queue.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_kstuff.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_log.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_manual.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_mdbg.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_mount_device.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_path_state.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_path_utils.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_scan.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_scan_tree.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_scanner.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_shellcore_flags.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_stability.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_time.c"
    "${SHADOWMOUNT_SOURCE_DIR}/src/sm_title_state.c")
set(SQLITE_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c")

onion_add_plugin(
    NAME ${ONION_PLUGIN_TARGET}
    TITLE_ID ${ONION_PLUGIN_ID}
    VERSION ${ONION_PLUGIN_VERSION}
    SOURCES
        source/main.c
        source/plugin_descriptor.c
        source/plugin_ui.c
        source/shadowmount_core.c
        source/shadowmount_paths.c
        source/shadowmount_service.c
        ${SHADOWMOUNT_SOURCES}
        ${SQLITE_SOURCE}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${SHADOWMOUNT_SOURCE_DIR}"
        "${SHADOWMOUNT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite"
    LIBRARIES
        SceLibcInternal
        SceSystemService
        SceUserService
        SceAppInstUtil
        SceNotification
        kernel_sys)

target_compile_definitions(${ONION_PLUGIN_TARGET} PRIVATE
    SHADOWMOUNT_VERSION="${SHADOWMOUNT_UPSTREAM_VERSION}"
    SHADOWMOUNT_CONFIG_TEMPLATE="${SHADOWMOUNT_SOURCE_DIR}/config.ini.example"
    SHADOWMOUNT_NOTIFICATION_ICON="${SHADOWMOUNT_SOURCE_DIR}/smp_icon.png")
set_source_files_properties(${SHADOWMOUNT_SOURCES} ${SQLITE_SOURCE}
    PROPERTIES COMPILE_FLAGS
    "-Wno-error -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare")
