# Configure

We use CMake and its Makefile generator to build the flexVDI guest tools. At this moment, the officially supported toolchains are GCC >= 4.8 for Linux and MinGW64 for Windows cross-compilation.

Use the following variables to control the build process:

- CMAKE_BUILD_TYPE, to select between debug and release builds. See [https://cmake.org/cmake/help/v2.8.12/cmake.html#variable:CMAKE_BUILD_TYPE]
- BUILD_CLIENT=No, to disable building the client-side library.
- BUILD_GUEST=No, to disable building the guest-side agent and tools.
- CMAKE_TOOLCHAIN_FILE, to select a different toolchain than the default. Use the files cmake/Windows32.cmake and cmake/Windows64.cmake to cross-compile for Windows in 32 and 64 bit architectures, using the MinGW64 toolchain.

The build system will disable the client-side and/or guest-side targets if their dependencies are not found.

# Build

Once configured, build the flexVDI guest tools with 'make'.

For Linux targets, the installer is built with 'make package'.

For Windows targets, the installer is built with 'make installer'. It will:

- Download the latest Virtio drivers for Windows.
- Look for the 32 and 64 bit versions of the Windows vdagent. It will first climb up two directory levels and then look down recursively. So, if the current build directory is 'a/b/c', the vdagent binaries must have at least 'a' as their ancestor.
- Use these two elements to build the Spice guest tools installer.
- Similarly, look for the 32 and 64 bit versions of the flexVDI guest tools.
- Build the flexVDI guest tools installer. In this way, it is a 32-bit executable that installs in both the 32 and 64-bit version of Windows.

For Windows targets, the guest tools ISO is built with 'make guest-tools-iso'. It depends on the installer.

