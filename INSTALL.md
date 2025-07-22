# Installing pngcheck

## Requirements

- Standard C compiler
- CMake 3.14 or later
- zlib library or compatible (e.g. zlib-ng)

## Build Instructions

### Using CMake (recommended)

#### Unix/Linux/etc.

1. Prepare the build directory through the CMake preset:

   ```sh
   cmake --preset Release
   ```

2. Configure if necessary with one of these options:

   ```sh
   # Default build with the system-installed zlib (if available)
   # No additional configuration needed

   # Build with a locally-downloaded zlib
   cmake -DPNGCHECK_USE_SYSTEM_ZLIB=OFF --preset Release
   ```

3. Build:

   ```sh
   cmake --build build --target pngcheck --preset Release
   ```

4. Install (optional):

   ```sh
   # Depending your system setup, you might need to use 'sudo'
   cmake --install build --preset Release
   ```

#### Windows (MSVC with vcpkg)

1. Install zlib via vcpkg:

   ```cmd
   vcpkg install zlib:x64-windows
   # For ARM64: vcpkg install zlib:arm64-windows
   ```

2. Configure and build:

   ```cmd
   cmake -B build -A x64 --preset Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   # For ARM64: cmake -B build -A ARM64 --preset Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --preset Release
   ```

#### Windows (MSYS2/MinGW)

1. Install dependencies in MSYS2:

   ```sh
   pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-zlib mingw-w64-x86_64-cmake
   # For 32-bit: mingw-w64-i686-toolchain mingw-w64-i686-zlib mingw-w64-i686-cmake
   ```

2. Configure and build:

   ```sh
   cmake -B build -G "MSYS Makefiles" --preset Release
   cmake --build build --preset Release
   ```

#### macOS (with Homebrew zlib)

If using Homebrew-installed zlib instead of system zlib:

1. Install zlib via Homebrew:

   ```sh
   brew install zlib
   ```

2. Configure and build (same as generic instructions above)

### Using Make (the traditional method)

* With the system-installed zlib (if available):

  ```sh
  make
  ```

* With zlib installed in a non-standard location, using dynamic linking,
  forcing compilation with gcc:

  ```sh
  make CC="gcc" \
       CPPFLAGS="-I/path/to/zlib" \
       LDFLAGS="-L/path/to/zlib"
  ```

* With zlib installed in a non-standard location, using static linking,
  forcing compilation with clang at the highest optimization level targeting the local machine:

  ```sh
  make CC="clang" \
       CFLAGS="-O3 -march=native -mtune=native" \
       CPPFLAGS="-I/path/to/zlib" \
       LIBS="/path/to/zlib/libz.a"
  ```

### Using the compiler and linker directly (the "hard" method)

#### Unix/Linux/etc.

* With the system-installed zlib (if available):

  ```sh
  cc -Wall -O -o pngcheck pngcheck.c -lz
  ```

* With zlib installed in a non-standard location, using dynamic linking:

  ```sh
  cc -Wall -O -I/path/to/zlib -o pngcheck pngcheck.c -L/path/to/zlib -lz
  ```

* With zlib installed in a non-standard location, using static linking:

  ```sh
  cc -Wall -O -I/path/to/zlib -o pngcheck pngcheck.c /path/to/zlib/libz.a
  ```

#### Windows (MSVC)

* With zlib compiled as a Windows DLL (typically distributed as `zlib1.dll`):

  ```cmd
  cl -nologo -O2 -W3 -I\path\to\zlib -c pngcheck.c
  cl -nologo -O2 -W3 -c third_party\wildargs\wildargs.c
  link -nologo pngcheck.obj wildargs.obj \path\to\zlib\zdll.lib
  ```

  Copy `pngcheck.exe` and `zlib1.dll` to the installation directory.

* With zlib compiled as a static library:

  ```cmd
  cl -nologo -O2 -W3 -I\path\to\zlib -c pngcheck.c
  cl -nologo -O2 -W3 -c third_party\wildargs\wildargs.c
  link -nologo pngcheck.obj wildargs.obj \path\to\zlib\zlib.lib
  ```

  Copy `pngcheck.exe` to the installation directory.

## Notes

- CMake build is recommended as it handles dependencies and platform
  differences automatically.
- zlib support used to be optional, but now it is mandatory.
- Automatic wildcard argument expansion is supported on select non-Unix
  compilers and platforms. We currently support EMX/GCC on OS2, as well
  as Microsoft Visual C, Embarcadero Borland C, MinGW/GCC and Clang on
  Windows.

## More Information

- zlib: http://www.zlib.net/
- PNG/MNG/JNG: http://www.libpng.org/pub/png/
- pngcheck: http://www.libpng.org/pub/png/apps/pngcheck.html
