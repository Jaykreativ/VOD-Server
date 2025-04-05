cmake_minimum_required(VERSION 3.10)

project(VOD_Server)

file(GLOB VOD_Server_SRC
    "./src/*.cpp"
    "./src/*.h"
)

file(GLOB VOD_Server_Layers
    "./src/Layers/*.cpp"
    "./src/Layers/*.h"
)

file(GLOB VOD_Server_Shares
    "./src/Shares/*.cpp"
    "./src/Shares/*.h"
)

file(GLOB VOD_Server_Objects
    "./src/Objects/*.cpp"
    "./src/Objects/*.h"
)

source_group("Source" FILES ${VOD_Server_SRC})
source_group("Source/Layers" FILES  ${VOD_Server_Layers})
source_group("Source/Shares" FILES  ${VOD_Server_Shares})
source_group("Source/Objects" FILES  ${VOD_Server_Objects})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(VOD_Server ${VOD_Server_SRC} ${VOD_Server_Shares} ${VOD_Server_Layers} ${VOD_Server_Objects})

set_property(TARGET VOD_Server PROPERTY CXX_STANDARD 17)
set_property(TARGET VOD_Server PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(WIN32)
target_link_libraries(
	VOD_Server PUBLIC
	"ws2_32.lib"
)
endif(WIN32)
target_include_directories(
    VOD_Server PUBLIC
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
)