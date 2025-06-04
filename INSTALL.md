# Installing pngcheck

## Requirements

- Standard C compiler
- CMake 3.14 or later
- zlib library or compatible (e.g. zlib-ng)

## Build Instructions

### Using CMake (recommended)

1. Create a build directory:

   ```sh
   mkdir build && cd build
   ```

2. Configure with one of these options:

   ```sh
   # Default build with the system-installed zlib (if available)
   cmake ..

   # Build with a locally-downloaded zlib
   cmake -DPNGCHECK_USE_SYSTEM_ZLIB=OFF ..
   ```

3. Build:

   ```sh
   cmake --build .
   ```

4. Install (optional):

   ```sh
   # Depending your system setup, you might need to use 'sudo'
   cmake --install .
   ```

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
  link -nologo pngcheck.obj setargv.obj \path\to\zlib\zdll.lib
  ```

  Copy `pngcheck.exe` and `zlib1.dll` to the installation directory.

* With zlib compiled as a static library:

  ```cmd
  cl -nologo -O2 -W3 -I\path\to\zlib -c pngcheck.c
  link -nologo pngcheck.obj setargv.obj \path\to\zlib\zlib.lib
  ```

  Copy `pngcheck.exe` to the installation directory.

## Notes

- CMake build is recommended as it handles dependencies and platform
  differences automatically.
- zlib support used to be optional, but now it is mandatory.
- On Windows with MSVC, `setargv.obj` is included to handle file wildcards.
- For MinGW/gcc and Cygwin/gcc on Windows, as well as EMX/gcc on OS/2,
  wildcard argument handling is built-in.
- For other non-Unix compilers such as Borland C or Watcom C, wildcard
  argument handling is not currently supported.

## More Information

- zlib: http://www.zlib.net/
- PNG/MNG/JNG: http://www.libpng.org/pub/png/
- pngcheck: http://www.libpng.org/pub/png/apps/pngcheck.html
