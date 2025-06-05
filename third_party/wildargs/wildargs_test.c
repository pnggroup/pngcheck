/*
 * wildargs_test.c
 * Test program to be linked with wildargs.c
 *
 * Copyright (C) 2003-2025 Cosmin Truta.
 *
 * Use, modification and distribution are subject
 * to the Boost Software License, Version 1.0.
 * See the accompanying file LICENSE_BSL_1_0.txt
 * or visit https://www.boost.org/LICENSE_1_0.txt
 *
 * SPDX-License-Identifier: BSL-1.0
 */

#include <stdio.h>

int main(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; ++i)
        puts(argv[i]);
    return 0;
}
