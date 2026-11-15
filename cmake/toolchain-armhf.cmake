# Cross-compiling for 32-bit ARM hard-float (armhf) from an x86-64 Debian/Ubuntu: gcc-arm-linux-gnueabihf, the
# armhf libraries by multiarch (dpkg --add-architecture armhf), qemu-user to run the tests. The CPU is the build's
# own flags: CMAKE_CXX_FLAGS="-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard" (armv7), or
# "-march=armv7-a -mfpu=neon-vfpv4 -mtune=cortex-a7 -mfloat-abi=hard" (armv7 with NEON: a Raspberry Pi 2 and on).
#   CC=arm-linux-gnueabihf-gcc CXX=arm-linux-gnueabihf-g++ cmake -B build-arm \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-armhf.cmake -DCMAKE_CXX_FLAGS="..." -DCMAKE_C_FLAGS="..."
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # wayland-scanner and the like: the host's
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")
# the tests run under qemu-user, with the armhf libraries
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-arm;-L;/usr/arm-linux-gnueabihf)
