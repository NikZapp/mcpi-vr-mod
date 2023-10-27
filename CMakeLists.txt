cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Start Project
project(mcpivr-proj)
# Include SDK
include("$ENV{HOME}/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/sdk.cmake")
# Build
add_library(mcpivr SHARED main.cpp)
target_link_libraries(mcpivr mods-headers reborn-patch symbols misc)
add_custom_command(TARGET mcpivr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libmcpivr.so $ENV{HOME}/.minecraft-pi/mods/libmcpivr.so)