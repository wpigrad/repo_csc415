/**************************************************************
* Class::  CSC-415-01 Summer 2026
* Name:: Jonathan Jacobson
* Student ID:: 918856021
* GitHub-Name:: wpigrad
* Project:: Assignment 0 - Using Utility Header File
*
* File:: jacobson.c
*
* Description:: Test program demonstrating the use of the
*               validate_argc() and debug_print() functions
*               from jacobson.h
*
**************************************************************/
#include <stdio.h>          // fprintf(), printf()
#include <stdlib.h>         // exit()
#include "jacobson.h"       // debug_print(), validate_argc()

#define ARGC_MIN    1       // minimum number accepted
#define ARGC_MAX    3       // maximum number accepted
#define DEBUG       1       // DEBUG flag: 1=ON and 0=OFF

int main(int argc, char *argv[])
{
    if (DEBUG)
        debug_print("DEBUG ON\n");

    // validate the number of arguments -- exits on failure
    validate_argc(argc, argv, ARGC_MIN, ARGC_MAX);

    if (DEBUG)
        debug_print("validate_argc() passed successfully\n");

    // if we get here, argc is valid
    printf("argc is valid.\n");

    if (DEBUG)
        debug_print("Number of arguments: %d\n", argc);

    for (int i = 0; i < argc; i++)
    {
        if (DEBUG)
            debug_print("argv[%d] = %s\n", i, argv[i]);
    }

    if (DEBUG)
        debug_print("main() complete\n");

    return 0;
}
