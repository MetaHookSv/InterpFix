cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(InterpFix LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "InterpFix requires Windows MSVC x86. Configure with -A Win32.")
endif()
get_property(interpfix_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(interpfix_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select Debug or Release.")
endif()
include(cmake/Dependencies.cmake)
interpfix_prepare_dependencies()
include("${VC_LTL_Root}/VC-LTL helper for cmake.cmake")
include(cmake/SDK.cmake)
add_library(InterpFix SHARED
    src/plugins.cpp src/privatehook.cpp src/interpolation.cpp
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
interpfix_use_sdk(InterpFix)
target_compile_definitions(InterpFix PRIVATE WIN32 _WINDOWS _USRDLL _MBCS)
target_compile_options(InterpFix PRIVATE /MP /Zi /W3 /wd4311 /wd4312 /wd4819 /wd4996)
target_link_options(InterpFix PRIVATE /DEBUG
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/INCREMENTAL:NO>
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>)
set_target_properties(InterpFix PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
install(TARGETS InterpFix RUNTIME DESTINATION svencoop/metahook/plugins)
install(FILES "$<TARGET_PDB_FILE:InterpFix>" DESTINATION svencoop/metahook/plugins)

option(INTERPFIX_SYNC_GAMEDATA "Download and prune InterpFix gamedata before building" ON)
set(INTERPFIX_GAMEDATA_DIR "${PROJECT_BINARY_DIR}/assets/svencoop/metahook/gamedata/interpfix"
    CACHE PATH "InterpFix gamedata catalog directory")
if(INTERPFIX_SYNC_GAMEDATA)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_target(InterpFixGameData
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sync-gamedata.py"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/scripts/manifests/interpfix.json"
            --target-dir "${INTERPFIX_GAMEDATA_DIR}" --temp-root "${PROJECT_BINARY_DIR}/gamedata-sync"
        COMMENT "Synchronizing InterpFix gamedata" VERBATIM)
    add_custom_target(InterpFixGameDataValidate
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate-gamedata.py"
            "${INTERPFIX_GAMEDATA_DIR}" --manifest "${CMAKE_CURRENT_SOURCE_DIR}/scripts/manifests/interpfix.json"
        COMMENT "Validating InterpFix gamedata" VERBATIM)
    add_dependencies(InterpFixGameDataValidate InterpFixGameData)
    add_dependencies(InterpFix InterpFixGameDataValidate)
endif()
install(DIRECTORY "${INTERPFIX_GAMEDATA_DIR}/" DESTINATION svencoop/metahook/gamedata/interpfix OPTIONAL)

option(INTERPFIX_BUILD_TESTS "Build InterpFix regression tests" OFF)
if(INTERPFIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(METAHOOKSV_ENABLE_LAUNCH_GAME "Add the Visual Studio build/deploy/debug target" OFF)
    if(METAHOOKSV_ENABLE_LAUNCH_GAME)
        include(cmake/LaunchGame.cmake)
        metahooksv_add_launch_game()
    endif()
endif()
