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

#include <stdio.h>      //printf()
#include <stdlib.h>     //exit()

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
        fprintf(stderr, "Minimum arguments required: %d\n", argc_min - 1);
        exit(1);
    }

    if (argc > argc_max)
    {
        fprintf(stderr, "usage: %s [OPTIONS]\n", argv[0]);
        fprintf(stderr, "Too many arguments.\n");
        fprintf(stderr, "Maximum arguments allowed: %d\n", argc_max - 1);
        exit(1);
    }

    return 0;
}

#endif
