# Installing pngcheck

## Requirements

- Standard C compiler
- CMake 3.14 or later
- zlib library (optional but recommended)

## Build Instructions

### Using CMake (recommended)

1. Create a build directory:

   ```sh
   mkdir build && cd build
   ```

2. Configure with one of these options:

   ```sh
   # Default build with system zlib
   cmake ..

   # Build without zlib support
   cmake -DPNGCHECK_USE_ZLIB=OFF ..

   # Build with downloaded zlib
   cmake -DPNGCHECK_USE_SYSTEM_ZLIB=OFF ..
   ```

3. Build:

   ```sh
   cmake --build .
   ```

4. Install (optional):

   ```sh
   sudo cmake --install .
   ```

### Using Make (traditional method)

#### Unix/Linux/macOS

* Without zlib:

  ```sh
  gcc -Wall -O -o pngcheck pngcheck.c
  ```

* With zlib (recommended):

  ```sh
  gcc -Wall -O -DUSE_ZLIB -o pngcheck pngcheck.c -lz
  ```

* With zlib installed in a non-standard location, using dynamic linking:

  ```sh
  gcc -Wall -O -DUSE_ZLIB -I/path/to/zlib -o pngcheck pngcheck.c -L/path/to/zlib -lz
  ```

* With zlib installed in a non-standard location, using static linking:

  ```sh
  gcc -Wall -O -DUSE_ZLIB -I/path/to/zlib -o pngcheck pngcheck.c /path/to/zlib/libz.a
  ```

#### Windows (MSVC)

* Without zlib:

  ```cmd
  cl -nologo -O -W3 -DWIN32 -c pngcheck.c
  link -nologo pngcheck.obj setargv.obj
  ```

* With zlib:

  ```cmd
  cl -nologo -O -W3 -DWIN32 -DUSE_ZLIB -I/path/to/zlib -c pngcheck.c
  link -nologo pngcheck.obj setargv.obj \path\to\zlib\zlib.lib
  ```

Note: Copy pngcheck.exe and zlib.dll to the installation directory.

## Notes

- CMake build is recommended as it handles dependencies and platform differences automatically.
- zlib support is recommended and enabled by default.
- On Windows with MSVC, setargv.obj is included to handle file wildcards.
- For MinGW/gcc on Windows or emx/gcc on OS/2, wildcard handling is built-in.

## More Information

- zlib: http://www.zlib.net/
- PNG/MNG/JNG: http://www.libpng.org/pub/png/
- pngcheck: http://www.libpng.org/pub/png/apps/pngcheck.html
