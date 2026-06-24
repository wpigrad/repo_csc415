/**************************************************************
* Class::  CSC-415-01 Summer 2026
* Name:: Jonathan Jacobson
* Student ID:: 918856021
* GitHub-Name:: wpigrad
* Project:: Assignment # - Processing FLR Data with Threads
*
* File:: create_flr.c
*
* Description:: Creates a Fixed Length Record (FLR) binary data
*   file and its corresponding header file. Fields are stored as
*   null-terminated ASCII strings padded to a fixed width.
*   Fields: first_name (20), gender (3), age (5), time_in (12),
*           time_out (12), color (10)
*   Record length = sum of all field widths = 62 bytes.
*
**************************************************************/

#include <fcntl.h>         // open()
#include <stdio.h>         // fclose(), fopen(), fprintf(), printf()
#include <stdlib.h>        // free(), malloc()
#include <string.h>        // strncpy()
#include <sys/stat.h>      // S_IRGRP, S_IROTH, S_IRUSR, S_IWUSR
#include <unistd.h>        // close(), pread(), read(), write()

#define FIELD_COUNT    6
#define NUM_EMPLOYEES  5
#define DEBUG          1

int main(void)
{

///////////////////////////////////////////////////////////////////////////////

    // Declare variables
    int index_field, index_emp, offset, len_rec, fd, file_offset;
    int col_width = 10;
    char *ptr_rec_buf;
    FILE *ptr_header_file;
    int count_bytes_read;

///////////////////////////////////////////////////////////////////////////////

    // Define field names
    // These are the column names that will be written to the header file
    char *ptr_field_names[FIELD_COUNT] = {"first_name",
                                          "gender",
                                          "age",
                                          "time_in",
                                          "time_out",
                                          "color"
                                         };

///////////////////////////////////////////////////////////////////////////////

    // Define field widths. All records are the same size.
    // Each number is the number of bytes reserved for that field
    int field_widths[FIELD_COUNT] = { 20, 3, 5, 12, 12, 10 };

///////////////////////////////////////////////////////////////////////////////

    // Define employee data
    char *ptr_employees[NUM_EMPLOYEES][FIELD_COUNT] = {
        { "Amy", "F", "28", "08:00 AM", "05:00 PM", "Red"   },
        { "Bob", "M", "34", "09:15 AM", "06:30 PM", "White" },
        { "Eve", "F", "41", "07:30 AM", "04:00 PM", "Blue"  },
        { "Ian", "M", "25", "10:00 AM", "07:00 PM", "Red"   },
        { "Zoe", "F", "52", "08:45 AM", "05:45 PM", "White" }
    };

///////////////////////////////////////////////////////////////////////////////

    if (DEBUG) printf("[DEBUG] ptr_field_names populated "
                      "with %d fields\n", FIELD_COUNT);
    if (DEBUG) printf("[DEBUG] ptr_employees populated "
                      "with %d records\n", NUM_EMPLOYEES);

///////////////////////////////////////////////////////////////////////////////

    // Calculate fixed length record
    // Add up all the field widths to get the total byte size of one record
    len_rec = 0;
    for (index_field = 0; index_field < FIELD_COUNT; index_field++)
    {
        len_rec += field_widths[index_field];
    }

    if (DEBUG) printf("[DEBUG] len_rec calculated: %d bytes\n", len_rec);

///////////////////////////////////////////////////////////////////////////////

    // Write the header file using fopen() and fprintf()
    // fopen() returns a FILE* pointer which fprintf() needs.
    ptr_header_file = fopen("employees_header.txt", "w");
    if (ptr_header_file == NULL)
    {
        printf("Error opening header file\n");
        return 1;
    }

    // fprintf() converts the integer width to ASCII and
    // writes the full line directly to the file in one step.
    // (ie: "20: first_name\n")
    for (index_field = 0; index_field < FIELD_COUNT; index_field++)
    {
        fprintf(ptr_header_file, "%d: %s\n",
                field_widths[index_field],
                ptr_field_names[index_field]
               );
    }

    fclose(ptr_header_file);
    printf("Header written to employees_header.txt\n");

    if (DEBUG)
    {
        printf("[DEBUG] header file closed after "
               "writing %d fields\n", FIELD_COUNT
              );
    }

///////////////////////////////////////////////////////////////////////////////

    // Open the data file for writing
    // open() returns a file descriptor (fd)
    fd = open("employees.dat",
              O_WRONLY |  // open for writing only, not reading
              O_CREAT  |  // create the file if it does not exist
              O_TRUNC,    // if file exists, wipe it and start fresh
              S_IRUSR  |  // owner has read permission
              S_IWUSR  |  // owner has write permission
              S_IRGRP  |  // group has read permission
              S_IROTH     // others have read permission (rw-r--r--)
             );

    if (fd < 0)
    {
        printf("Error opening data file\n");
        return 1;
    }

    if (DEBUG) printf("[DEBUG] data file opened for writing\n");

///////////////////////////////////////////////////////////////////////////////

    // Allocate a buffer to hold one record,
    // malloc reserves a block of memory of len_rec bytes, and
    // reuse this same buffer for every employee record.
    ptr_rec_buf = malloc(len_rec);

///////////////////////////////////////////////////////////////////////////////

    // Write one record per employee,
    // copy each field value into its fixed position,
    // then write the whole buffer to the file as one complete record.
    for (index_emp = 0; index_emp < NUM_EMPLOYEES; index_emp++)
    {
        // Copy each field into the correct position in the buffer.
        // offset tracks where the next field starts.
        offset = 0;
        for (index_field = 0; index_field < FIELD_COUNT; index_field++)
        {
            // Copy the field value
            // leave room for the null terminator at the end of the string
            strncpy(ptr_rec_buf + offset,
                    ptr_employees[index_emp][index_field],
                    field_widths[index_field] - 1
                   );

            // Move offset forward by the field width
            offset += field_widths[index_field];
        }

        // Write the completed record buffer to the file
        write(fd, ptr_rec_buf, len_rec);

        if (DEBUG) printf("[DEBUG] wrote record %d: "
                          "%s, %s, %s, %s, %s, %s\n",
                          index_emp, ptr_employees[index_emp][0],
                          ptr_employees[index_emp][1],
                          ptr_employees[index_emp][2],
                          ptr_employees[index_emp][3],
                          ptr_employees[index_emp][4],
                          ptr_employees[index_emp][5]
                         );
    }

///////////////////////////////////////////////////////////////////////////////

    // Clean up
    // free() releases the memory we reserved with malloc.
    // close() closes the file descriptor when we are done.
    free(ptr_rec_buf);
    close(fd);

    printf("Data written to employees.dat "
           "(%d records x %d bytes)\n",
           NUM_EMPLOYEES, len_rec);

    if (DEBUG)
    {
        printf("[DEBUG] data file closed\n");
        printf("[DEBUG] total bytes written = %d\n",
               NUM_EMPLOYEES * len_rec);
    }

///////////////////////////////////////////////////////////////////////////////

    // Read the data file back to verify it
    // Open the file and read each record to confirm the data.
    // O_RDONLY has no permission flags because we are only reading
    // (no need to set write or create permissions)
    fd = open("employees.dat",
              O_RDONLY    // open for reading only
             );

    if (fd < 0)
    {
        printf("Error opening data file for reading\n");
        return 1;
    }

    if (DEBUG) printf("[DEBUG] data file opened for reading\n");

    // Allocate a fresh buffer to read records into
    ptr_rec_buf = malloc(len_rec);

    printf("Read verification\n");

    // Print the column header row
    for (index_field = 0; index_field < FIELD_COUNT; index_field++)
    {
        printf("%-*s  ", col_width, ptr_field_names[index_field]);
    }
    printf("\n");

    // Start at the beginning of the file and move forward
    // by one record length after each read
    file_offset = 0;

    // Read and print one record per employee.
    // pread reads from a specific byte offset in the file
    // without changing the file position, which makes it
    // easy to jump directly to any record by number.
    for (index_emp = 0; index_emp < NUM_EMPLOYEES; index_emp++)
    {
        // Read exactly one record starting at file_offset
        count_bytes_read = pread(fd, ptr_rec_buf, len_rec,
                                 file_offset);
        if (count_bytes_read != len_rec)
        {
            printf("Error reading record %d\n", index_emp);
            break;
        }

        // Move offset forward by one record length
        file_offset += len_rec;

        // Print each field from its fixed position in the record buffer
        offset = 0;
        for (index_field = 0; index_field < FIELD_COUNT; index_field++)
        {
            printf("%-*s  ", col_width, ptr_rec_buf + offset);
            offset += field_widths[index_field];
        }
        printf("\n");
    }

///////////////////////////////////////////////////////////////////////////////

    // Final cleanup
    free(ptr_rec_buf);
    close(fd);

    if (DEBUG) printf("[DEBUG] memory freed, file closed, "
                      "return success\n");

    return 0;
}
