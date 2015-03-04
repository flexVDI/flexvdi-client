# Toolchain file to cross-compile for Windows 32-bit targets.
# It is based on Fedoras's mingw-w64.

# this one is important
set(CMAKE_SYSTEM_NAME Windows)
set(WIN_ARCH i686)
set(WIN_BITS 32)

# specify the cross compiler
set(CMAKE_C_COMPILER   /usr/bin/${WIN_ARCH}-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/${WIN_ARCH}-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  /usr/bin/${WIN_ARCH}-w64-mingw32-windres)

# here is the target environment located
set(CMAKE_FIND_ROOT_PATH /usr/${WIN_ARCH}-w64-mingw32/sys-root/mingw)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# adjust paths for pkg-config
set(ENV{PKG_CONFIG_LIBDIR} ${CMAKE_FIND_ROOT_PATH}/lib/pkgconfig:${CMAKE_FIND_ROOT_PATH}/share/pkgconfig)
