cmake_minimum_required(VERSION "3.16")

set(PRODUCTION_BUILD OFF CACHE BOOL "Make this a production build" FORCE)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")

if(PRODUCTION_BUILD)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

project("mc-server")
set(CMAKE_CXX_STANDARD 20)

# Add external library here ->
add_subdirectory("thirdParty/asio")

file (GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(${CMAKE_PROJECT_NAME} "${MY_SOURCES}")

if(MSVC)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE asio)
