# Native 32-bit Linux build (i686) with clang and the multilib libraries.
# The game code still assumes a 32-bit memory layout (struct sizes, file formats), so it's built as 32-bit.
#
#   cmake -S . -B build-linux -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-linux-i686.cmake -DJONES3D_RENDERER=Vulkan
#
# Requires: lib32-glibc, lib32-gcc-libs, lib32-sdl3, lib32-vulkan-icd-loader, lib32-gtk3 (wxWidgets)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_C_FLAGS_INIT "-m32")
set(CMAKE_CXX_FLAGS_INIT "-m32")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-m32")

# Find the 32-bit libraries and pkg-config files
set(CMAKE_LIBRARY_ARCHITECTURE "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
list(APPEND CMAKE_PREFIX_PATH /usr/lib32)
set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib32 ${CMAKE_SYSTEM_LIBRARY_PATH})
set(FIND_LIBRARY_USE_LIB32_PATHS ON)
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib32/pkgconfig:/usr/share/pkgconfig")
