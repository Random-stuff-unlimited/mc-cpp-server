# =============================================================================
# mc-server - Minecraft C++ Server
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(mc-server
    VERSION 1.0.0
    DESCRIPTION "Minecraft Server Implementation in C++"
    LANGUAGES CXX
)

# =============================================================================
# Build Configuration
# =============================================================================

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# =============================================================================
# Dependencies
# =============================================================================

include(FetchContent)

# Try to find system zlib first, otherwise download it
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    message(STATUS "System zlib not found, downloading via FetchContent...")
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
    )
    # Prevent zlib from installing itself
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zlib)
    
    # Create an alias target to match the system zlib interface
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
else()
    message(STATUS "Found system zlib: ${ZLIB_VERSION_STRING}")
endif()

# =============================================================================
# Source Files
# =============================================================================

# Automatically discover source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

# =============================================================================
# Main Executable
# =============================================================================

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/data
    ${CMAKE_SOURCE_DIR}/include/network
    ${CMAKE_SOURCE_DIR}/include/world
    ${CMAKE_SOURCE_DIR}/include/lib
)

# Link libraries
if(ZLIB_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${zlib_SOURCE_DIR}
        ${zlib_BINARY_DIR}
    )
endif()

# Platform-specific configurations
if(WIN32)
    # Windows socket library
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# =============================================================================
# Compiler Warnings
# =============================================================================

include(cmake/CompilerWarnings.cmake)
set_project_warnings(${PROJECT_NAME})

# =============================================================================
# Build Type Specific Flags
# =============================================================================

# Debug definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
)

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# =============================================================================
# Project Information
# =============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Source files:  ${CMAKE_SOURCE_DIR}/src")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
