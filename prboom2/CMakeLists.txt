cmake_minimum_required(VERSION 3.9)

option(WITH_IMAGE "Use SDL2_image if available" ON)
if(WITH_IMAGE)
    list(APPEND VCPKG_MANIFEST_FEATURES "sdl2-image")
endif()

# Automatically install dependencies
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL TRUE)

project("dsda-doom" VERSION 0.27.5)

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckBigEndian)
check_big_endian(WORDS_BIGENDIAN)

include(GNUInstallDirs)

if(POLICY CMP0099)
  cmake_policy(SET CMP0099 NEW)
else()
  message(WARNING "Your version of CMake is very old. This may cause linking issues if your dependencies are not in your compiler's default search paths.")
endif()

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG "Search for package config before using Find modules" ON)

if(VCPKG_TOOLCHAIN)
    set(ENV{PKG_CONFIG_PATH} "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig")
endif()

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT HAVE_LTO)

include(CMakeDependentOption)
cmake_dependent_option(ENABLE_LTO "Use link-time optimisation" OFF "HAVE_LTO" OFF)

set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "dsda-doom")
set(WAD_DATA "dsda-doom.wad")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")

include(CheckSymbolExists)

check_symbol_exists(stricmp "string.h" HAVE_STRICMP)
check_symbol_exists(strnicmp "string.h" HAVE_STRNICMP)
check_symbol_exists(getopt "unistd.h" HAVE_GETOPT)
check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
check_symbol_exists(CreateFileMapping "windows.h" HAVE_CREATE_FILE_MAPPING)
check_symbol_exists(strsignal "string.h" HAVE_STRSIGNAL)
check_symbol_exists(mkstemp "stdlib.h" HAVE_MKSTEMP)

include(CheckIncludeFile)

check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("asm/byteorder.h" HAVE_ASM_BYTEORDER_H)
check_include_file("dirent.h" HAVE_DIRENT_H)

include(GetAvailableFlags)

get_supported_warnings(SUPPORTED_WARNINGS)
get_fast_math_flag(FAST_MATH_FLAG)
get_compile_definitions(DSDA_COMPILE_DEFINITIONS)
get_deprecation_silencing_definitions(DEPRECATION_SILENCING_DEFINITIONS)

include(PkgConfigHelper)

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL 2.0 REQUIRED)
find_package(libzip REQUIRED)
find_package(SDL2 2.0.7 REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(ZLIB REQUIRED)

if(NOT TARGET SDL2_mixer::SDL2_mixer)
    add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_mixer::SDL2_mixer-static)
endif()

if(WITH_IMAGE)
    find_package(SDL2_image)
    if(SDL2_image_FOUND)
        set(HAVE_LIBSDL2_IMAGE TRUE)
        if(NOT TARGET SDL2_image::SDL2_image)
            add_library(SDL2_image::SDL2_image ALIAS SDL2_image::SDL2_image-static)
        endif()
    endif()
endif()

if(WIN32)
    set(DEFAULT_WAD_DIR ".")
else()
    set(DEFAULT_WAD_DIR "${CMAKE_INSTALL_DATADIR}/games/doom")
endif()

set(DSDAPWADDIR "${DEFAULT_WAD_DIR}" CACHE STRING "Path to install DSDA-Doom internal WAD, relative to CMAKE_INSTALL_PREFIX or absolute.")
set(DOOMWADDIR "${CMAKE_INSTALL_PREFIX}/${DEFAULT_WAD_DIR}" CACHE PATH "Path to look for WAD files.")

if(IS_ABSOLUTE "${DSDAPWADDIR}")
    set(DSDA_ABSOLUTE_PWAD_PATH "${DSDAPWADDIR}")
else()
    set(DSDA_ABSOLUTE_PWAD_PATH "${CMAKE_INSTALL_PREFIX}/${DSDAPWADDIR}")
endif()

option(SIMPLECHECKS "Enable checks which only impose significant overhead if a posible error is detected" ON)

# Debug options, disabled by default
option(RANGECHECK "Enable internal range checking" OFF)

configure_file(cmake/config.h.cin config.h)

# Support cross compiling
option(FORCE_CROSSCOMPILE "Enable cross-compilation" OFF)
if(FORCE_CROSSCOMPILE)
    set(CMAKE_CROSSCOMPILING ON)
endif()

if(CMAKE_CROSSCOMPILING)
    set(IMPORT_EXECUTABLES "IMPORTFILE-NOTFOUND" CACHE FILEPATH "Export file from native build")
    include(${IMPORT_EXECUTABLES})
else()
    if(NOT CROSS_EXPORTS)
        set(CROSS_EXPORTS "")
    endif()
endif()

set(DSDA_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(WAD_DATA_PATH "${DSDA_OUTPUT_PATH}/${WAD_DATA}")

add_subdirectory(data)
add_subdirectory(src)

if(NOT CMAKE_CROSSCOMPILING)
    export(TARGETS ${CROSS_EXPORTS} FILE "${CMAKE_BINARY_DIR}/ImportExecutables.cmake")
endif()

if(WIN32)
    include(CPack)
endif()
