# SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
# SPDX-License-Identifier: BSD-3-Clause


cmake_minimum_required(VERSION 3.18)
project(ru.kotdath.AuroraRL3D CXX)

find_package (Qt5 COMPONENTS Core Qml Gui Quick OpenGL LinguistTools REQUIRED)

include(FindPkgConfig)
pkg_search_module(AURORA auroraapp REQUIRED)
pkg_search_module(AURORA auroraapp_i18n REQUIRED)

pkg_check_modules(RLTOOLS REQUIRED IMPORTED_TARGET rltools)
pkg_check_modules(OPENBLAS REQUIRED IMPORTED_TARGET openblas)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (default Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

option(AURORA_RL3D_ENABLE_FAST_MATH "Enable fast math flags for RL training builds" ON)
option(AURORA_RL3D_ENABLE_NATIVE_ARCH "Enable -march=native (disable for cross/PSDK builds)" OFF)
option(AURORA_RL3D_ENABLE_ARM_OPTIMIZATIONS "Enable ARM-specific optimizations for Aurora OS" ON)
# Threads for BLAS/OMP (can be overridden via -DOPENBLAS_NUM_THREADS / -DOMP_NUM_THREADS)
set(OPENBLAS_NUM_THREADS 4 CACHE STRING "Default OpenBLAS threads")
set(OMP_NUM_THREADS 1 CACHE STRING "Default OMP threads")

set(AURORA_RL3D_RELEASE_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(AURORA_RL3D_ENABLE_FAST_MATH)
        list(APPEND AURORA_RL3D_RELEASE_FLAGS -Ofast -ffast-math)
    else()
        list(APPEND AURORA_RL3D_RELEASE_FLAGS -O3 -fno-fast-math -fno-finite-math-only)
    endif()
    if(AURORA_RL3D_ENABLE_NATIVE_ARCH)
        list(APPEND AURORA_RL3D_RELEASE_FLAGS -march=native)
    endif()

    # ARM-specific optimizations for Aurora OS
    if(AURORA_RL3D_ENABLE_ARM_OPTIMIZATIONS)
#        if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
#            list(APPEND AURORA_RL3D_RELEASE_FLAGS -mfpu=neon -mfloat-abi=hard)
#        endif()
#        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
#            list(APPEND AURORA_RL3D_RELEASE_FLAGS -mcpu=cortex-a76 -mtune=cortex-a76)
#        endif()
#        # Common ARM optimizations
#        list(APPEND AURORA_RL3D_RELEASE_FLAGS -fno-strict-aliasing -funroll-loops)
    endif()
endif()

# Configure OpenBLAS for Qt compatibility (avoid threading conflicts)
# Parallelism target: 4 env threads, keep BLAS/OMP modest to avoid oversubscription on 4C/8T CPU.
set(ENV{OPENBLAS_NUM_THREADS} "8")
set(ENV{OMP_NUM_THREADS} "1")

set(SHORT_NAME AuroraRL3D)

file(GLOB_RECURSE SOURCES_FILES CONFIGURE_DEPENDS "src/*.cpp")

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "src/*.h")

file(GLOB_RECURSE QML_FILES CONFIGURE_DEPENDS "qml/*.qml")

file(GLOB_RECURSE ICON_FILES CONFIGURE_DEPENDS "icons/*.png")

file(GLOB TS_FILES CONFIGURE_DEPENDS "translations/*.ts")

file(GLOB MD_FILES CONFIGURE_DEPENDS "*.md")

set(TRANSLATION_SOURCES_FILES
    ${SOURCES_FILES}
    ${QML_FILES}
)

set(GEN_TS_PATH ${CMAKE_CURRENT_BINARY_DIR}/gen_translations)

foreach(_ts_file ${TS_FILES})
    file(COPY ${_ts_file} DESTINATION ${GEN_TS_PATH})
endforeach()
file(GLOB GEN_TS_FILES "${GEN_TS_PATH}/*.ts")

add_custom_target(update_translations)
foreach(_ts_file ${GEN_TS_FILES})
    get_filename_component(_ts_name ${_ts_file} NAME_WE)
    add_custom_command(
        OUTPUT ${_ts_file}
        DEPENDS ${TRANSLATION_SOURCES_FILES}
        COMMAND ${Qt5_LUPDATE_EXECUTABLE}
        ARGS ${TRANSLATION_SOURCES_FILES} -ts ${_ts_file}
        COMMAND ${CMAKE_COMMAND} -E copy
        ARGS ${_ts_file} ${CMAKE_CURRENT_SOURCE_DIR}/translations
    )
    add_custom_target(translate_${_ts_name} DEPENDS ${_ts_file})
    add_dependencies(update_translations translate_${_ts_name})
endforeach()

qt5_add_translation(TRANSLATION_QM_FILES ${GEN_TS_FILES})

set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/lib")

add_executable(${PROJECT_NAME}
    ${SOURCES_FILES}
    ${HEADER_FILES}
    ${ICON_FILES}
    ${QML_FILES}
    ${TS_FILES}
    ${PROJECT_NAME}.desktop
    rpm/${PROJECT_NAME}.spec
    ${TRANSLATION_QM_FILES}
    ${MD_FILES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
    RL_TOOLS_BACKEND_ENABLE_OPENBLAS
    RL_TOOLS_BACKEND_ENABLE_BLAS
    RL_TOOLS_RL_ENVIRONMENTS_ENABLE_MUJOCO
    # OpenBLAS threading configuration for Qt compatibility
    OPENBLAS_NUM_THREADS=${OPENBLAS_NUM_THREADS}
    OMP_NUM_THREADS=${OMP_NUM_THREADS}
)
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
    $<BUILD_INTERFACE:
    ${AURORA_INCLUDE_DIRS}
    ${RLTOOLS_INCLUDE_DIRS}
>)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Quick
    Qt5::OpenGL
    Qt5::Gui
    -lGLESv2
    -lEGL
    ${AURORA_LDFLAGS}
    PkgConfig::RLTOOLS
    PkgConfig::OPENBLAS
)
if(AURORA_RL3D_ENABLE_FAST_MATH AND AURORA_RL3D_RELEASE_FLAGS)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${AURORA_RL3D_RELEASE_FLAGS}>
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY qml
    DESTINATION share/${PROJECT_NAME}
)

foreach(_file IN LISTS TRANSLATION_QM_FILES)
    get_filename_component(_filename "${_file}" NAME)
    string(REPLACE "${SHORT_NAME}" "${PROJECT_NAME}" _full_filename "${_filename}")
    install(FILES ${_file} DESTINATION share/${PROJECT_NAME}/translations RENAME ${_full_filename})
endforeach()

set(ICON_SIZES 86x86 108x108 128x128 172x172)
foreach(_size IN LISTS ICON_SIZES)
    install(FILES icons/${_size}/${PROJECT_NAME}.png DESTINATION share/icons/hicolor/${_size}/apps)
endforeach()

install(FILES ${PROJECT_NAME}.desktop
    DESTINATION share/applications
)

file(WRITE "${CMAKE_BINARY_DIR}/QtCreatorDeployment.txt"
    "${CMAKE_INSTALL_PREFIX}
${CMAKE_BINARY_DIR}/${PROJECT_NAME}:bin
")
