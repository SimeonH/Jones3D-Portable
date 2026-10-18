cmake_minimum_required(VERSION 3.24)
project(SDL3_external C)

# SDL3 is the platform layer (window, input, timing) of the non-Windows builds.
# The system library is used (lib32-sdl3 for the 32-bit Linux build).
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared GLOBAL)
message(STATUS "Using SDL3 ${SDL3_VERSION}")
