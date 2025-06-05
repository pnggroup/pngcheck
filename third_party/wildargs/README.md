wildargs
========

Automatic command-line wildcard expansion for C and C++
environments that aren't based on the Un\*x shell.


Usage
-----

Simply compile and link the wildargs.c module to the rest
of the program modules, as in the following example:

    cc -O2 -o wildargs_test wildargs_test.c wildargs.c

Check the output to ensure that the wildcards are expanded.
In this example, expect to see a listing of the `*.c` files:

    ./wildargs_test *.c


Compatibility
-------------

The following runtimes are recognized and supported on
Windows:

 * Microsoft Universal C Runtime (UCRT)
 * Microsoft Visual C/C++ Runtime (MSVCRT)
 * Borland (Embarcadero) C/C++ Runtime Library (RTL)

On Unix and Unix-like systems, the wildargs module is,
conventionally, a no-op.

Supporting other compilers and runtimes, on systems and
platforms not necessarily restricted to Windows, is TODO.
