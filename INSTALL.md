# Configure

We use CMake and its Makefile generator to build the flexVDI Spice client library. At this moment, the officially supported toolchains are GCC >= 4.8 for Linux and MinGW64 for Windows cross-compilation.

Use the following variables to control the build process:

- CMAKE_BUILD_TYPE, to select between debug and release builds. See [https://cmake.org/cmake/help/v2.8.12/cmake.html#variable:CMAKE_BUILD_TYPE]
- CMAKE_TOOLCHAIN_FILE, to select a different toolchain than the default. Use the files cmake/Windows32.cmake and cmake/Windows64.cmake to cross-compile for Windows in 32 and 64 bit architectures, using the MinGW64 toolchain.

# Build

Once configured, build with 'make'.
