/**************************************************************
* Class::  CSC-415-01 Summer 2026
* Name:: Jonathan Jacobson
* Student ID:: 918856021
* GitHub-Name:: wpigrad
* Project:: Assignment 0 - Utility Header File
*
* File:: jacobson.h
*
* Description:: Personal utility header file for reusable
*               functions across projects.
*
**************************************************************/
#ifndef JACOBSON_H
#define JACOBSON_H

#include <stdarg.h>     // va_end(), va_list(), va_start()
#include <stdio.h>      // fprintf(), printf(), vprintf()
#include <stdlib.h>     // exit()

/**************************************************************
* Function:: validate_argc
*
* Description:: Validates the number of command-line arguments
*               passed to the program. Prints a generic usage
*               message and exits with failure if the number of
*               arguments is outside the allowed range. Returns
*               0 on success.
*
* Parameters::
*   argc     -- the argument count passed to main()
*   argv     -- the argument vector passed to main()
*   argc_min -- the minimum number of arguments allowed
*   argc_max -- the maximum number of arguments allowed
*
* Returns:: 0 on success, exits with 1 on failure
*
**************************************************************/
int validate_argc(int argc, char *argv[], int argc_min, int argc_max)
{
    if (argc < argc_min)
    {
        fprintf(stderr, "usage: %s [OPTIONS]\n", argv[0]);
        fprintf(stderr, "Too few arguments.\n");
        fprintf(stderr, "Minimum arguments required: %d\n",
                argc_min - 1);
        exit(1);
    }

    if (argc > argc_max)
    {
        fprintf(stderr, "usage: %s [OPTIONS]\n", argv[0]);
        fprintf(stderr, "Too many arguments.\n");
        fprintf(stderr, "Maximum arguments allowed: %d\n",
                argc_max - 1);
        exit(1);
    }

    return 0;
}

/**************************************************************
* Function:: debug_print
*
* Description:: Prints a debug message surrounded by a blank
*               line above and below, and bordered by rows
*               of repeated border characters above and below
*               the message. The border character, length, and
*               thickness are set by local variables and can
*               each be changed in one place. Accepts a format
*               string and optional arguments exactly like
*               printf(). Intended to be called when DEBUG is
*               ON to make debug output visually distinct and
*               easy to locate in a busy terminal.
*
* Parameters::
*   message -- the format string to print between the borders
*   ...     -- optional arguments matching the format string
*
* Returns:: 0 on success
*
**************************************************************/
int debug_print(char *message, ...)
{
    int  i;
    int  row;
    char border        = '/';   // change to use a different character
    int  border_length = 79;    // change to use a different length
    int  border_thick  = 2;     // change to use a different thickness

    // va_list holds the optional arguments passed after message
    va_list args;

    printf("\n");

    // print border_thick rows of border_length characters above
    for (row = 0; row < border_thick; row++)
    {
        for (i = 0; i < border_length; i++)
        {
            printf("%c", border);
        }
        printf("\n");
    }

    printf("\n");

    // va_start() initializes args to point to the first
    // optional argument after message
    va_start(args, message);

    // vprintf() works like printf() but accepts a va_list
    // instead of individual arguments
    vprintf(message, args);

    // va_end() cleans up the va_list when we are done with it
    va_end(args);

    printf("\n");

    // print border_thick rows of border_length characters below
    for (row = 0; row < border_thick; row++)
    {
        for (i = 0; i < border_length; i++)
        {
            printf("%c", border);
        }
        printf("\n");
    }

    printf("\n");

    return 0;
}

#endif
