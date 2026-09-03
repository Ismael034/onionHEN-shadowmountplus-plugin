cmake_minimum_required(VERSION 3.20)

project(OnionHENPluginBoilerplate VERSION 1.0.0 LANGUAGES C)

# Plugin identity. Keep these values stable after publishing a plugin.
set(ONION_PLUGIN_TARGET example_plugin)
set(ONION_PLUGIN_ID ONIO10001)
set(ONION_PLUGIN_VERSION 1.00)
set(ONION_PLUGIN_NAME "Example Plugin")

set(ONIONHEN_PLUGIN_SDK_SOURCE "" CACHE PATH
    "Use a local onionHEN-plugin-sdk checkout instead of downloading it")
set(ONIONHEN_PLUGIN_SDK_GIT_REPOSITORY
    "https://github.com/OnionBuddies/onionHEN-plugin-sdk.git" CACHE STRING
    "OnionHEN Plugin SDK repository")
set(ONIONHEN_PLUGIN_SDK_GIT_TAG
    "0cf4346536705fb271ba0eb2226d97ebcb23559c" CACHE STRING
    "Pinned OnionHEN Plugin SDK revision")

if(NOT ONION_PLUGIN_ID MATCHES "^[A-Za-z][A-Za-z][A-Za-z][A-Za-z][0-9][0-9][0-9][0-9][0-9]$")
    message(FATAL_ERROR
        "ONION_PLUGIN_ID must contain four letters followed by five digits")
endif()
if(NOT ONION_PLUGIN_VERSION MATCHES "^[0-9]+\\.[0-9][0-9]$")
    message(FATAL_ERROR "ONION_PLUGIN_VERSION must use the N.NN format")
endif()

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

onion_add_plugin(
    NAME ${ONION_PLUGIN_TARGET}
    TITLE_ID ${ONION_PLUGIN_ID}
    VERSION ${ONION_PLUGIN_VERSION}
    SOURCES
        source/main.c
        source/plugin_descriptor.c
        source/plugin_ui.c
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    LIBRARIES
        SceLibcInternal
        kernel_sys)
