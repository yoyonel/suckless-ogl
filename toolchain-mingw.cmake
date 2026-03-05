# CMake Toolchain file for Cross-Compiling to Windows using MinGW
#
# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Target triple
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Cross compilers
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Search paths for dependencies
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Adjust the search behavior:
# Search for programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Optional: Link everything statically for easier deployment under Wine/Windows
# set(CMAKE_EXE_LINKER_FLAGS "-static")
