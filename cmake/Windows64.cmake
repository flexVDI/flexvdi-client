# Toolchain file to cross-compile for Windows 64-bit targets.

set(CMAKE_SYSTEM_NAME Windows)
set(WIN_ARCH x86_64)
set(WIN_BITS 64)

set(CMAKE_C_COMPILER   /usr/bin/${WIN_ARCH}-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/${WIN_ARCH}-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  /usr/bin/${WIN_ARCH}-w64-mingw32-windres)

# Find the system root
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-sysroot
                OUTPUT_VARIABLE MINGW_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND find "${MINGW_SYSROOT}" -name lib
                OUTPUT_VARIABLE MINGW_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(REPLACE "/lib" "" MINGW_SYSROOT "${MINGW_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH
    /usr/local/${WIN_ARCH}-w64-mingw32
    "${MINGW_SYSROOT}")

# Adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Adjust paths for pkg-config
set(ENV{PKG_CONFIG_PATH} "")
foreach (_DIR IN LISTS CMAKE_FIND_ROOT_PATH)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${_DIR}/lib/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${_DIR}/share/pkgconfig")
endforeach (_DIR)
