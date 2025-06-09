wildargs
========

Automatic command-line wildcard expansion for platforms and
environments that aren't based on the Un\*x shell.


Usage
-----

Simply compile and link the `wildargs.c` module to the rest of
the program:

    cl -O2 wildargs_test.c wildargs.c

Run the program and check the expansion of wildcard arguments.
In this example, you should see a listing of the `*.c` files:

    wildargs_test *.c


Compatibility
-------------

The following compilers and runtimes are currently supported
on Windows:

 * Microsoft Visual C++, supporting MSVCRT and UCRT+VCRUNTIME;
 * MinGW/MinGW32 and MinGW-w64, supporting MSVCRT and UCRT;
 * Borland (Embarcadero) C/C++, supporting Borland RTL.

On Unix and Unix-like systems, the wildargs module is a no-op.

Supporting other compilers, runtimes, and even platforms other
than Windows, is TODO.
