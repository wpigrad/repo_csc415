/**************************************************************
* Class::  CSC-415-01 Summer 2026
* Name:: Jonathan Jacobson
* Student ID:: 918856021
* GitHub-Name:: wpigrad
* Project:: Assignment # - Processing FLR Data with Threads
*
* File:: final.c
*
* Description:: Merges standalone.c (runtime header parsing and
*   command line argument handling) with store_stats.c (single
*   pass record processing, per-call-type bucketing, and stats
*   calculation). All field offsets and sizes are derived from
*   the header file at runtime. The data file, thread count,
*   subfield, and subfield values all come from the command
*   line. This merge step is single threaded; threading comes
*   later.
*
**************************************************************/

#include <math.h>          // sqrt()
#include <fcntl.h>         // open()
#include <stdio.h>         // printf(), fflush()
#include <stdlib.h>        // malloc(), free(), atoi(), exit()
#include <string.h>        // strcmp(), strncmp(), strncpy()
#include <sys/stat.h>      // fstat(), struct stat
#include <time.h>          // struct tm, mktime(), difftime(), time_t
#include <unistd.h>        // read(), close(), write(), pread()

#define MAX_FIELDS   64    // chose safe power of 2 (num of headers = 36)
#define MAX_NAME_LEN 64    // chose safe power of 2 (longest field  = 35)

#define UNIQUE_CALL_TYPES          162   // based on EDA of Law_Full.dat
#define MAX_CALL_TYPE_PERCENT       60   // based on EDA of Law_Full.dat (53%)

#define CITY_LABEL                "SF"
#define INT_MAX_VALUE       2147483647  // maximum value 4-byte signed integer
#define TIME_DIFF_MAX    INT_MAX_VALUE  // 1 day=86,400 sec

#define DEBUG        1

///////////////////////////////////////////////////////////////////////////////

// print_arrays()
// USAGE: print_arrays(arr_offsets, arr_widths, arr_names, count)
// DESCRIPTION: Prints the offsets, widths, and names arrays in DEBUG format.
// RETURNS: void
// EXAMPLE: print_arrays(arr_field_offsets, arr_field_widths, arr_field_names,
//                       total_fields);
void print_arrays(int *arr_offsets, int *arr_widths,
                  char arr_names[][MAX_NAME_LEN], int count)
{
    int index_t;   // index_t for total array
    for (index_t = 0; index_t < count; index_t++)
        printf("arr_offsets[%d]=%d  arr_widths[%d]=%d  "
               "arr_names[%d]=%s\n",
               index_t, arr_offsets[index_t],
               index_t, arr_widths[index_t],
               index_t, arr_names[index_t]);
    printf("\n");
}

///////////////////////////////////////////////////////////////////////////////

// print_arr_table()
// USAGE: print_arr_table(arr_offsets, arr_widths, arr_names, count, label)
// DESCRIPTION: Prints a formatted table of offsets, widths, and names.
// RETURNS: void
// EXAMPLE: print_arr_table(arr_field_offsets, arr_field_widths,
//                          arr_field_names, total_fields, "Total fields");
void print_arr_table(int *arr_offsets, int *arr_widths,
                     char arr_names[][MAX_NAME_LEN], int count, char *label)
{
    printf("%-4s  %-6s  %-8s  %s\n",
           "num", "offset", "width", "field_name");
    printf("%-4s  %-6s  %-8s  %s\n",
           "----", "------", "-----", "----------");

    int index_t;   // index_t for total array
    for (index_t = 0; index_t < count; index_t++)
    {
        printf("%-4d  %-6d  %-8d  %s\n",
               index_t + 1,
               arr_offsets[index_t],
               arr_widths[index_t],
               arr_names[index_t]);
    }

    printf("\n%s : %d\n\n", label, count);
}

///////////////////////////////////////////////////////////////////////////////

// str_to_struct_tm()
// USAGE: str_to_struct_tm("MM/DD/YYYY HH:MM:SS AM/PM")
// DESCRIPTION: Converts a datetime string into a struct tm.
// RETURNS: a struct tm with all the fields filled in.
// EXAMPLE: struct tm struct_tm_datetime = str_to_struct_tm("02/23/2022 05:15:24 PM");
struct tm str_to_struct_tm(char *ptr_datetime)
{
    int  month, day, year;
    int  hour;  // 24 hour clock
    int  min;
    int  sec;
    char ampm;  // only care about A or P

    // ptr_datetime: 02/23/2022 05:15:24 PM
    // index       : 0123456789012345678901

    month = atoi(&ptr_datetime[0]);
    day   = atoi(&ptr_datetime[3]);
    year  = atoi(&ptr_datetime[6]);
    hour  = atoi(&ptr_datetime[11]);
    min   = atoi(&ptr_datetime[14]);
    sec   = atoi(&ptr_datetime[17]);
    ampm  = ptr_datetime[20];

    // Convert from 12 to 24 hour clock
    if (ampm == 'P')
    {
        if (hour != 12)
            hour = hour + 12;   // example: 5 PM becomes 5 + 12 = 17
    }
    else
    {
        if (hour == 12)
            hour = 0;           // 12 AM becomes midnight = 0
    }

    // Populate and return the struct tm
    struct tm struct_tm_datetime;
    struct_tm_datetime.tm_sec   = sec;         // Seconds [0, 60]
    struct_tm_datetime.tm_min   = min;         // Minutes [0, 59]
    struct_tm_datetime.tm_hour  = hour;        // Hours   [0, 23]
    struct_tm_datetime.tm_mday  = day;         // Day of the month [1, 31]
    struct_tm_datetime.tm_mon   = month - 1;   // Month of the year [0, 11]
    struct_tm_datetime.tm_year  = year - 1900; // Years from 1900
    struct_tm_datetime.tm_wday  = -1;          // -1 until mktime() fills it in
    struct_tm_datetime.tm_yday  = -1;          // -1 until mktime() fills it in
    struct_tm_datetime.tm_isdst = -1;          // -1 until mktime() fills it in

    return struct_tm_datetime;
}

///////////////////////////////////////////////////////////////////////////////

// print_struct_tm_to_str()
// USAGE: print_struct_tm_to_str(struct_tm_datetime)
// DESCRIPTION: Prints a struct tm as a datetime string.
// RETURNS: void
// EXAMPLE: print_struct_tm_to_str(struct_tm_datetime);
void print_struct_tm_to_str(struct tm struct_tm_datetime)
{
    printf("struct tm: %02d/%02d/%04d %02d:%02d:%02d  wday=%d yday=%d isdst=%d\n\n",
           struct_tm_datetime.tm_mon  + 1,    // Month of the year [0, 11]
           struct_tm_datetime.tm_mday,        // Day of the month [1, 31]
           struct_tm_datetime.tm_year + 1900, // Years from 1900
           struct_tm_datetime.tm_hour,        // Hours   [0, 23]
           struct_tm_datetime.tm_min,         // Minutes [0, 59]
           struct_tm_datetime.tm_sec,         // Seconds [0, 60]
           struct_tm_datetime.tm_wday,        // Day of the week  [0, 6]   (Sun)
           struct_tm_datetime.tm_yday,        // Day of the year  [0, 365] (Jan 1)
           struct_tm_datetime.tm_isdst);      // Daylight savings flag
}

///////////////////////////////////////////////////////////////////////////////

// closest_power_of_2()
// USAGE: closest_power_of_2(n)
// DESCRIPTION: Returns the smallest power of 2 that is >= n.
// RETURNS: int
// EXAMPLE: closest_power_of_2(9) returns 16
int closest_power_of_2(int n)
{
    int result = 1;
    while (result < n)
        result = result * 2;
    return result;
}

///////////////////////////////////////////////////////////////////////////////

// struct struct_call_type_stats holds all diff values for one call type
// members ordered largest to smallest to minimize padding
struct struct_call_type_stats
{
    int  *ptr_arr_diff_received_to_dispatch;  //  8 bytes (pointer on 64-bit)
    int  *ptr_arr_diff_enroute_to_onscene;    //  8 bytes (pointer on 64-bit)
    int  *ptr_arr_diff_received_to_onscene;   //  8 bytes (pointer on 64-bit)
    char *ptr_label;                          //  8 bytes "SF", "BAYVIEW", etc.
    char  ptr_call_type[MAX_NAME_LEN];        // 64 bytes
    int   count;                              //  4 bytes
};

///////////////////////////////////////////////////////////////////////////////

// struct struct_stats holds all calculated statistics for one diff array
// members ordered largest to smallest to minimize padding
struct struct_stats
{
    double mean;    // 8 bytes
    double stddev;  // 8 bytes
    int    count;   // 4 bytes
    int    lb;      // 4 bytes  lower bound = Q1 - 1.5 * IQR
    int    min;     // 4 bytes
    int    q1;      // 4 bytes
    int    median;  // 4 bytes
    int    q3;      // 4 bytes
    int    iqr;     // 4 bytes  interquartile range = Q3 - Q1
    int    max;     // 4 bytes
    int    ub;      // 4 bytes  upper bound = Q3 + 1.5 * IQR
                    // 56 bytes total
};

///////////////////////////////////////////////////////////////////////////////

// sort_arr()
// USAGE: sort_arr(ptr_arr, count)
// DESCRIPTION: Sorts an integer array in ascending order using
// selection sort. Works by finding the smallest element in the
// unsorted part and swapping it to the front.
// RETURNS: void
void sort_arr(int *ptr_arr, int count)
{
    int index_i;
    int index_j;
    int index_min;
    int temp;

    for (index_i = 0; index_i < count - 1; index_i++)
    {
        // find the smallest element in the unsorted part
        index_min = index_i;
        for (index_j = index_i + 1; index_j < count; index_j++)
        {
            if (ptr_arr[index_j] < ptr_arr[index_min])
                index_min = index_j;
        }

        // swap it to the front of the unsorted part
        temp               = ptr_arr[index_i];
        ptr_arr[index_i]   = ptr_arr[index_min];
        ptr_arr[index_min] = temp;
    }
}

///////////////////////////////////////////////////////////////////////////////

// calc_stats()
// USAGE: calc_stats(ptr_arr, count)
// DESCRIPTION: Calculates statistics on a sorted integer array.
// Array must be sorted before calling this function.
// RETURNS: struct struct_stats with all statistics filled in
struct struct_stats calc_stats(int *ptr_arr, int count)
{
    struct struct_stats stats;
    stats.count = count;

    // min and max are first and last elements of sorted array
    stats.min = ptr_arr[0];
    stats.max = ptr_arr[count - 1];

    // median: middle element (or average of two middle elements)
    if (count % 2 == 0)
        stats.median = (ptr_arr[count/2 - 1] + ptr_arr[count/2]) / 2;
    else
        stats.median = ptr_arr[count/2];

    // Q1: median of the lower half
    int lower_count = count / 2;
    if (lower_count % 2 == 0)
        stats.q1 = (ptr_arr[lower_count/2 - 1] + ptr_arr[lower_count/2]) / 2;
    else
        stats.q1 = ptr_arr[lower_count/2];

    // Q3: median of the upper half
    int upper_start = (count % 2 == 0) ? count/2 : count/2 + 1;
    int upper_count = count - upper_start;
    if (upper_count % 2 == 0)
        stats.q3 = (ptr_arr[upper_start + upper_count/2 - 1] +
                    ptr_arr[upper_start + upper_count/2]) / 2;
    else
        stats.q3 = ptr_arr[upper_start + upper_count/2];

    // IQR, lower bound, upper bound
    stats.iqr = stats.q3 - stats.q1;
    stats.lb  = stats.q1 - (int)(1.5 * stats.iqr);
    stats.ub  = stats.q3 + (int)(1.5 * stats.iqr);

    // mean
    double sum = 0.0;
    int index_i;
    for (index_i = 0; index_i < count; index_i++)
        sum += ptr_arr[index_i];
    stats.mean = sum / count;

    // stddev: square root of the average squared deviation from the mean
    double sum_sq = 0.0;
    for (index_i = 0; index_i < count; index_i++)
    {
        double diff = ptr_arr[index_i] - stats.mean;
        sum_sq += diff * diff;
    }
    stats.stddev = sqrt(sum_sq / count);

    return stats;
}

///////////////////////////////////////////////////////////////////////////////

// print_stats_table()
// USAGE: print_stats_table(ptr_arr_stats, num_call_types, ptr_label, diff_index)
// DESCRIPTION: Prints a stats table for all call types matching ptr_label
// for the specified diff array (0=received_to_dispatch, 1=enroute_to_onscene,
// 2=received_to_onscene)
// RETURNS: void
void print_stats_table(struct struct_call_type_stats *ptr_arr_stats,
                       int num_call_types, char *ptr_label, int diff_index)
{
    if (diff_index == 0)
        printf("\n%s : received to dispatch\n", ptr_label);
    else if (diff_index == 1)
        printf("\n%s : enroute to onscene\n", ptr_label);
    else
        printf("\n%s : received to onscene\n", ptr_label);

    printf("%25s | %6s | %6s | %6s | %6s | %6s | %9s | %6s | %6s | %9s | %6s | %8s\n",
           "Call Type", "count", "min", "LB", "Q1", "med", "mean",
           "Q3", "UB", "max", "IQR", "stddev");
    printf("%25s-+-%6s-+-%6s-+-%6s-+-%6s-+-%6s-+-%9s-+-%6s-+-%6s-+-%9s-+-%6s-+-%8s\n",
           "-------------------------", "------", "------", "------",
           "------", "------", "---------", "------", "------",
           "---------", "------", "--------");

    int index_c;
    for (index_c = 0; index_c < num_call_types; index_c++)
    {
        if (strcmp(ptr_arr_stats[index_c].ptr_label, ptr_label) != 0)
            continue;

        int *ptr_arr;
        if (diff_index == 0)
            ptr_arr = ptr_arr_stats[index_c].ptr_arr_diff_received_to_dispatch;
        else if (diff_index == 1)
            ptr_arr = ptr_arr_stats[index_c].ptr_arr_diff_enroute_to_onscene;
        else
            ptr_arr = ptr_arr_stats[index_c].ptr_arr_diff_received_to_onscene;
        int    count   = ptr_arr_stats[index_c].count;

        sort_arr(ptr_arr, count);
        struct struct_stats s = calc_stats(ptr_arr, count);

        printf("%25s | %6d | %6d | %6d | %6d | %6d | %9.2f | %6d | %6d | %9d | %6d | %8.2f\n",
               ptr_arr_stats[index_c].ptr_call_type,
               s.count, s.min, s.lb, s.q1, s.median, s.mean,
               s.q3, s.ub, s.max, s.iqr, s.stddev);
    }
}

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{

// section 1 //////////////////////////////////////////////////////////////////

    // Parse and store command line arguments.
    if (argc != 7)
    {
        printf("usage: %s [DATA FILE][HEADER FILE][THREADS]"
               "[SUBFIELD][SUBFIELD VALUE1][SUBFIELD VALUE2]\n", argv[0]);
        printf("command [arguments]\n");
        printf("<DATA FILE>       Law[5|10|50|100|500]K.dat\n");
        printf("<HEADER FILE>     USE header.txt\n");
        printf("<THREADS>         USE number of threads [1, 4]\n");
        printf("<SUBFIELD>        USE [police_district | analysis_neighborhood]\n");
        printf("<SUBFIELD VALUE1> USE [BAYVIEW         | 'Russian Hill']\n");
        printf("<SUBFIELD VALUE2> USE [SOUTHERN        | 'Outer Richmond']\n");
        exit(1);
    }

    char *ptr_data_file    = argv[1];       // data filename
    char *ptr_header_file  = argv[2];       // header filename
    int   num_threads      = atoi(argv[3]); // number of threads
    char *ptr_subfield     = argv[4];       // subfield to filter on
    char *ptr_subval1      = argv[5];       // subfield value 1
    char *ptr_subval2      = argv[6];       // subfield value 2

    if (DEBUG)
    {
        printf("[DEBUG] arguments parsed:\n");
        printf("[DEBUG] ptr_data_file   = %s\n", ptr_data_file);
        printf("[DEBUG] ptr_header_file = %s\n", ptr_header_file);
        printf("[DEBUG] num_threads     = %d\n", num_threads);
        printf("[DEBUG] ptr_subfield    = %s\n", ptr_subfield);
        printf("[DEBUG] ptr_subval1     = %s\n", ptr_subval1);
        printf("[DEBUG] ptr_subval2     = %s\n\n", ptr_subval2);
    }

// section 2 //////////////////////////////////////////////////////////////////
// preprocessing //////////////////////////////////////////////////////////////

    // Hardcoded list of fields to keep.
    // Loop through arr_field_names to find them so the order
    // in the data file does not matter.
    char *ptr_selected_fields[] = {
        "received_datetime",
        "dispatch_datetime",
        "enroute_datetime",
        "onscene_datetime",
        "call_type_original_desc",
        "call_type_final_desc",
        "analysis_neighborhood",
        "police_district"
    };
    // 64 bytes total / 8 bytes per element = 8 elements
    int num_sel_fields = sizeof(ptr_selected_fields) /
                         sizeof(ptr_selected_fields[0]);

// section 3 //////////////////////////////////////////////////////////////////

    // Declare variables
    int fd_header;
    // struct stat is a container defined in <sys/stat.h>
    // fstat() gives access to information about an open file
    struct stat file_info;

// section 4 //////////////////////////////////////////////////////////////////

    // Open the header file for reading
    fd_header = open(ptr_header_file,
              O_RDONLY    // open for reading only
             );

    // return value check
    if (fd_header < 0)
    {
        printf("Error: could not open %s\n", ptr_header_file);
        return 1;
    }

    if (DEBUG) printf("[DEBUG] fd_header = %d : %s opened successfully\n\n",
                      fd_header, ptr_header_file
                     );

// section 5 //////////////////////////////////////////////////////////////////

    // Use fstat() to get file information without reading the file.
    // fstat() fills a struct stat with details about the open file.
    // Use the st_size field which holds the total size in bytes.

    // combine the function call with the return value check
    if (fstat(fd_header, &file_info) < 0)
    {
        printf("Error: fstat() failed\n");
        close(fd_header);
        return 1;
    }

    if (DEBUG)
    {
        printf("[DEBUG] fstat() fields:\n");
        printf("[DEBUG] st_dev     = %-15ld ( device file lives on )\n",
               file_info.st_dev);
        printf("[DEBUG] st_ino     = %-15ld ( unique inode number )\n",
               file_info.st_ino);
        printf("[DEBUG] st_nlink   = %-15ld ( number of hard links )\n",
               (long)file_info.st_nlink);
        printf("[DEBUG] st_mode    = %-15o ( permissions in octal )\n",
               file_info.st_mode);
        printf("[DEBUG] st_uid     = %-15d ( user ID of file owner )\n",
               file_info.st_uid);
        printf("[DEBUG] st_gid     = %-15d ( group ID of file owner )\n",
               file_info.st_gid);
        printf("[DEBUG] st_rdev    = %-15ld ( device number )\n",
               file_info.st_rdev);
        printf("[DEBUG] st_size    = %-15ld ( total size in bytes )\n",
               file_info.st_size);
        printf("[DEBUG] st_blksize = %-15ld ( best block size for reading )\n",
               (long)file_info.st_blksize);
        printf("[DEBUG] st_blocks  = %-15ld ( 512-byte blocks on disk )\n",
               file_info.st_blocks);
        printf("[DEBUG] st_atime   = %-15ld ( last time file was read )\n",
               file_info.st_atime);
        printf("[DEBUG] st_mtime   = %-15ld ( last time file was modified )\n",
               file_info.st_mtime);
        printf("[DEBUG] st_ctime   = %-15ld ( permissions changed )\n\n",
               file_info.st_ctime);

        // Print the file size
        printf("File size: %ld bytes\n\n", file_info.st_size);
    }

// section 6 //////////////////////////////////////////////////////////////////

    // Read the entire header file into a buffer using read().
    // Use st_size from fstat() to know how many bytes to read.
    int   header_bytes;
    int   read_bytes;
    char  header_buf[file_info.st_size + 1];

    header_bytes = file_info.st_size;
    read_bytes = read(fd_header, header_buf, file_info.st_size);
    if (read_bytes != header_bytes)
    {
        printf("Error: could not read %s\n", ptr_header_file);
        return 1;
    }

    header_buf[header_bytes] = '\0';   // null-terminate the buffer
    close(fd_header);

    if (DEBUG)
    {
        printf("[DEBUG] C string: \n%s\n", header_buf);
        // printf() is buffered, write() goes directly to stdout
        printf("[DEBUG] ");
        fflush(stdout);       // flush printf buffer before write()
        write(1, header_buf+760, 10);
        printf("\n");
        printf("[DEBUG] read %d bytes from %s\n\n",
               header_bytes, ptr_header_file);
    }

// section 7 //////////////////////////////////////////////////////////////////

    // strtok() splits the C string on any of the three delimiters:
    //   ':'  separates the width from the field name
    //   ' '  the space after the colon
    //   '\n' the newline at the end of each line
    // assumes no spaces in field names
    int  arr_field_widths[MAX_FIELDS];
    char arr_field_names[MAX_FIELDS][MAX_NAME_LEN];
    int  arr_field_offsets[MAX_FIELDS];

    int   total_fields = 0;
    int   field_offset = 0;
    char *ptr_token;

    ptr_token = strtok(header_buf, ": \n");
    while (1)
    {
        // stop if strtok() ran out of tokens
        if (ptr_token == NULL)
            break;

        // stop if we have reached the maximum number of fields
        if (total_fields >= MAX_FIELDS)
            break;

        // first token of each pair is the width
        int width = atoi(ptr_token);

        // second token of each pair is the field name
        // Pass NULL to continue parsing the same string
        ptr_token = strtok(NULL, ": \n");
        if (ptr_token == NULL)
            break;

        arr_field_widths[total_fields]  = width;
        arr_field_offsets[total_fields] = field_offset;
        // Use strncpy() to control the number of bytes copied
        // The -1 leaves room for the '\0' null terminator at the end
        strncpy(arr_field_names[total_fields], ptr_token, MAX_NAME_LEN - 1);

        field_offset += width;
        total_fields++;

        // advance to the next pair
        ptr_token = strtok(NULL, ": \n");
    }

    // REC_LEN is the sum of all field widths from the header.
    // field_offset has accumulated that sum during the parse loop above.
    int rec_len = field_offset;

    if (DEBUG)
    {
        printf("[DEBUG] %d fields parsed from %s\n",
               total_fields, ptr_header_file);

        // verify all fields were parsed correctly from header.txt
        print_arrays(arr_field_offsets, arr_field_widths,
                     arr_field_names, total_fields);
        // print a table of total fields
        print_arr_table(arr_field_offsets, arr_field_widths,
                        arr_field_names, total_fields,
                        "Total fields"
                       );
        printf("[DEBUG] rec_len = %d (sum of all field widths)\n\n", rec_len);
    }

// section 8 //////////////////////////////////////////////////////////////////

    int  arr_sel_widths[num_sel_fields];
    char arr_sel_fields[num_sel_fields][MAX_NAME_LEN];
    int  arr_sel_offsets[num_sel_fields];
    int  num_sel_found = 0;

    int index_s;   // index_s for selected array
    for (index_s = 0; index_s < num_sel_fields; index_s++)
    {
        int index_t;   // index_t for total array
        for (index_t = 0; index_t < total_fields; index_t++)
        {
            if (strcmp(arr_field_names[index_t], ptr_selected_fields[index_s]) == 0)
            {
                arr_sel_widths[num_sel_found]   = arr_field_widths[index_t];
                arr_sel_offsets[num_sel_found]  = arr_field_offsets[index_t];
                strncpy(arr_sel_fields[num_sel_found],
                        arr_field_names[index_t], MAX_NAME_LEN - 1);
                num_sel_found++;
                break;
            }
        }
    }

    if (DEBUG)
    {
        printf("[DEBUG] %d fields parsed from %s\n",
               num_sel_found, ptr_header_file);

        // verify new arrays created for selected fields
        printf("[DEBUG] %d fields selected:\n", num_sel_found);
        print_arrays(arr_sel_offsets, arr_sel_widths,
                     arr_sel_fields, num_sel_found);
        // print a table of selected fields
        print_arr_table(arr_sel_offsets, arr_sel_widths,
                        arr_sel_fields, num_sel_found,
                        "Selected fields"
                       );
    }
// end of preprocessing ///////////////////////////////////////////////////////
// section 9 //////////////////////////////////////////////////////////////////

    // use direct index into arr_sel_offsets and arr_sel_widths
    // order matches ptr_selected_fields[] defined in section 2:
    // [0] received_datetime      [4] call_type_original_desc
    // [1] dispatch_datetime      [5] call_type_final_desc
    // [2] enroute_datetime       [6] analysis_neighborhood
    // [3] onscene_datetime       [7] police_district
    int offset_received           = arr_sel_offsets[0];
    int offset_dispatch           = arr_sel_offsets[1];
    int offset_enroute            = arr_sel_offsets[2];
    int offset_onscene            = arr_sel_offsets[3];
    int offset_call_type_original = arr_sel_offsets[4];
    int offset_call_type_final    = arr_sel_offsets[5];
    int offset_neighborhood       = arr_sel_offsets[6];
    int offset_district           = arr_sel_offsets[7];

    int size_datetime             = arr_sel_widths[0];
    int size_call_type_original   = arr_sel_widths[4];
    int size_call_type_final      = arr_sel_widths[5];
    int size_neighborhood         = arr_sel_widths[6];
    int size_district             = arr_sel_widths[7];

    if (DEBUG)
    {
        printf("[DEBUG] offset_received           = %d\n", offset_received);
        printf("[DEBUG] offset_dispatch           = %d\n", offset_dispatch);
        printf("[DEBUG] offset_enroute            = %d\n", offset_enroute);
        printf("[DEBUG] offset_onscene            = %d\n", offset_onscene);
        printf("[DEBUG] offset_call_type_original = %d\n", offset_call_type_original);
        printf("[DEBUG] offset_call_type_final    = %d\n", offset_call_type_final);
        printf("[DEBUG] offset_neighborhood       = %d\n", offset_neighborhood);
        printf("[DEBUG] offset_district           = %d\n", offset_district);
        printf("[DEBUG] size_datetime             = %d\n", size_datetime);
        printf("[DEBUG] size_call_type_original   = %d\n", size_call_type_original);
        printf("[DEBUG] size_call_type_final      = %d\n", size_call_type_final);
        printf("[DEBUG] size_neighborhood         = %d\n", size_neighborhood);
        printf("[DEBUG] size_district             = %d\n\n", size_district);
    }

// section 10 /////////////////////////////////////////////////////////////////

    // open the data file
    int fd_data = open(ptr_data_file, O_RDONLY);
    if (fd_data < 0)
    {
        printf("Error: could not open %s\n", ptr_data_file);
        return 1;
    }

    if (DEBUG) printf("[DEBUG] fd_data = %d : %s opened successfully\n\n",
                      fd_data, ptr_data_file);

// section 11 /////////////////////////////////////////////////////////////////

    // use fstat() to get the total file size
    struct stat struct_data_info;
    if (fstat(fd_data, &struct_data_info) < 0)
    {
        printf("Error: fstat() failed\n");
        close(fd_data);
        return 1;
    }

    int num_records = struct_data_info.st_size / rec_len;
    int max_records_per_call_type =
        closest_power_of_2(num_records*MAX_CALL_TYPE_PERCENT/100);

    if (DEBUG) printf("[DEBUG] file size   = %ld bytes\n",  struct_data_info.st_size);
    if (DEBUG) printf("[DEBUG] num_records = %d\n", num_records);
    if (DEBUG) printf("[DEBUG] max_records_per_call_type = %d  "
                      "(closest power of 2 >= %d%% of num_records)\n\n",
                      max_records_per_call_type, MAX_CALL_TYPE_PERCENT);

// section 12 /////////////////////////////////////////////////////////////////

    // allocate the array of call_type_stats structs on the heap
    // so all threads can access the shared data
    // multiply by 3 to account for SF, ptr_subval1, and ptr_subval2 groups
    int max_call_types = closest_power_of_2(UNIQUE_CALL_TYPES) * 3;
    int num_call_types = 0;

    struct struct_call_type_stats *ptr_arr_stats =
        malloc(max_call_types * sizeof(struct struct_call_type_stats));

    // sizeof() returns size_t which is an unsigned (z = size_t sized and u = unsigned)
    if (DEBUG) printf("[DEBUG] sizeof(struct struct_call_type_stats) = %zu bytes\n",
                      sizeof(struct struct_call_type_stats));

    if (ptr_arr_stats == NULL)
    {
        printf("Error: malloc() failed for ptr_arr_stats\n");
        close(fd_data);
        return 1;
    }

    if (DEBUG)
    {
        printf("[DEBUG] max_call_types = %d "
               "(closest power of 2 >= %d)\n",
               max_call_types, UNIQUE_CALL_TYPES);
        printf("[DEBUG] struct struct_call_type_stats breakdown:\n");
        printf("[DEBUG] ptr_arr_diff_received_to_dispatch : %zu bytes\n",
               sizeof(ptr_arr_stats[0].ptr_arr_diff_received_to_dispatch));
        printf("[DEBUG] ptr_arr_diff_enroute_to_onscene   : %zu bytes\n",
               sizeof(ptr_arr_stats[0].ptr_arr_diff_enroute_to_onscene));
        printf("[DEBUG] ptr_arr_diff_received_to_onscene  : %zu bytes\n",
               sizeof(ptr_arr_stats[0].ptr_arr_diff_received_to_onscene));
        printf("[DEBUG] ptr_label                         : %zu bytes\n",
               sizeof(ptr_arr_stats[0].ptr_label));
        printf("[DEBUG] ptr_call_type                     : %zu bytes\n",
               sizeof(ptr_arr_stats[0].ptr_call_type));
        printf("[DEBUG] count                             : %zu bytes\n",
               sizeof(ptr_arr_stats[0].count));
        printf("[DEBUG] total struct size                 : %zu bytes\n",
               sizeof(struct struct_call_type_stats));
        printf("[DEBUG] total malloc                      : %zu bytes\n",
               max_call_types * sizeof(struct struct_call_type_stats));
    }

    // malloc three diff arrays for each call type
    int index_c;   // index_c for call type array
    for (index_c = 0; index_c < max_call_types; index_c++)
    {
        ptr_arr_stats[index_c].count     = 0;
        ptr_arr_stats[index_c].ptr_label = NULL;   // set when bucket is created
        // EDA showed the largest time difference was under the int range of 2,147,483,647
        ptr_arr_stats[index_c].ptr_arr_diff_received_to_dispatch =
            malloc(max_records_per_call_type * sizeof(int));
        ptr_arr_stats[index_c].ptr_arr_diff_enroute_to_onscene  =
            malloc(max_records_per_call_type * sizeof(int));
        ptr_arr_stats[index_c].ptr_arr_diff_received_to_onscene =
            malloc(max_records_per_call_type * sizeof(int));

        if (ptr_arr_stats[index_c].ptr_arr_diff_received_to_dispatch == NULL ||
            ptr_arr_stats[index_c].ptr_arr_diff_enroute_to_onscene  == NULL ||
            ptr_arr_stats[index_c].ptr_arr_diff_received_to_onscene == NULL)
        {
            printf("Error: malloc() failed for bucket %d\n", index_c);
            close(fd_data);
            return 1;
        }
    }

    if (DEBUG) printf("[DEBUG] malloc %d call type buckets\n\n",
                      max_call_types);

// section 13 /////////////////////////////////////////////////////////////////

    // determine which offset and size to use based on ptr_subfield
    // offsets and sizes come from the header-parsed arrays (section 9)
    int offset_subfield;
    int size_subfield;

    if (strcmp(ptr_subfield, "police_district") == 0)
    {
        offset_subfield = offset_district;
        size_subfield   = size_district;
    }
    else if (strcmp(ptr_subfield, "analysis_neighborhood") == 0)
    {
        offset_subfield = offset_neighborhood;
        size_subfield   = size_neighborhood;
    }
    else
    {
        printf("Error: unknown SUBFIELD '%s'\n", ptr_subfield);
        close(fd_data);
        exit(1);
    }

    if (DEBUG) printf("[DEBUG] SUBFIELD = %s  offset = %d  size = %d\n\n",
                      ptr_subfield, offset_subfield, size_subfield);

    // single pass through the data file
    // for each record:
    // 1. read call_type, subfield, and four datetime fields
    // 2. convert datetimes to time_t and calculate diffs
    // 3. find or create a bucket for this call_type
    // 4. store the three diffs in the SF bucket (all records)
    // 5. also store in ptr_subval1 or ptr_subval2 bucket if matches
    // an array acts as a pointer to its first element when passed to a func
    char ptr_call_type_original[size_call_type_original + 1];
    char ptr_call_type_final[size_call_type_final + 1];
    char *ptr_call_type;
    char ptr_subfield_val[size_subfield + 1];  // value read from subfield column
    char ptr_received[size_datetime + 1];
    char ptr_dispatch[size_datetime + 1];
    char ptr_enroute[size_datetime + 1];
    char ptr_onscene[size_datetime + 1];

    int rec_num;
    for (rec_num = 0; rec_num < num_records; rec_num++)
    {
        int rec_offset = rec_num * rec_len;

        // read the fields for this record
        pread(fd_data, ptr_call_type_original, size_call_type_original,
              rec_offset + offset_call_type_original);
        pread(fd_data, ptr_call_type_final,    size_call_type_final,
              rec_offset + offset_call_type_final);
        pread(fd_data, ptr_subfield_val, size_subfield, rec_offset + offset_subfield);
        pread(fd_data, ptr_received,  size_datetime, rec_offset + offset_received);
        pread(fd_data, ptr_dispatch,  size_datetime, rec_offset + offset_dispatch);
        pread(fd_data, ptr_enroute,   size_datetime, rec_offset + offset_enroute);
        pread(fd_data, ptr_onscene,   size_datetime, rec_offset + offset_onscene);

        ptr_call_type_original[size_call_type_original] = '\0';
        ptr_call_type_final[size_call_type_final]       = '\0';
        ptr_subfield_val[size_subfield]                 = '\0';
        ptr_received[size_datetime]                     = '\0';
        ptr_dispatch[size_datetime]                     = '\0';
        ptr_enroute[size_datetime]                      = '\0';
        ptr_onscene[size_datetime]                      = '\0';

        // use ptr_call_type_final if available,
        // otherwise fall back to ptr_call_type_original
        // skip the record if both are blank
        if (ptr_call_type_final[0] != '\0')
            ptr_call_type = ptr_call_type_final;
        else if (ptr_call_type_original[0] != '\0')
            ptr_call_type = ptr_call_type_original;
        else
            continue;

        // skip records with missing datetime fields
        if (ptr_received[0] == '\0' || ptr_dispatch[0] == '\0' ||
            ptr_enroute[0]  == '\0' || ptr_onscene[0]  == '\0')
            continue;

        // convert datetime strings to time_t
        struct tm struct_tm_received = str_to_struct_tm(ptr_received);
        struct tm struct_tm_dispatch = str_to_struct_tm(ptr_dispatch);
        struct tm struct_tm_enroute  = str_to_struct_tm(ptr_enroute);
        struct tm struct_tm_onscene  = str_to_struct_tm(ptr_onscene);

        time_t time_received = mktime(&struct_tm_received);
        time_t time_dispatch = mktime(&struct_tm_dispatch);
        time_t time_enroute  = mktime(&struct_tm_enroute);
        time_t time_onscene  = mktime(&struct_tm_onscene);

        // calculate the three diffs in seconds
        int diff_received_to_dispatch = (int)difftime(time_dispatch, time_received);
        int diff_enroute_to_onscene   = (int)difftime(time_onscene,  time_enroute);
        int diff_received_to_onscene  = (int)difftime(time_onscene,  time_received);

        // skip records with time differences greater than TIME_DIFF_MAX (bad data)
        if (diff_received_to_dispatch > TIME_DIFF_MAX ||
            diff_enroute_to_onscene   > TIME_DIFF_MAX ||
            diff_received_to_onscene  > TIME_DIFF_MAX)
            continue;

        // skip records with negative or zero time differences (bad data)
        if (diff_received_to_dispatch <= 0 ||
            diff_enroute_to_onscene   <= 0 ||
            diff_received_to_onscene  <= 0)
            continue;

        // store the diffs in the bucket
        // count is also the index of the next open position in the arrays
        // so you don't have to keep track of which position is next to be filled

        // find or create SF bucket and store (all records go into SF)
        int found_sf = 0;
        int index_sf = -1;
        for (index_c = 0; index_c < num_call_types; index_c++)
        {
            if (strcmp(ptr_arr_stats[index_c].ptr_call_type, ptr_call_type) == 0 &&
                strcmp(ptr_arr_stats[index_c].ptr_label, CITY_LABEL) == 0)
            {
                found_sf = 1;
                index_sf = index_c;
                break;
            }
        }
        if (!found_sf)
        {
            strncpy(ptr_arr_stats[num_call_types].ptr_call_type,
                    ptr_call_type, MAX_NAME_LEN - 1);
            ptr_arr_stats[num_call_types].ptr_call_type[MAX_NAME_LEN - 1] = '\0';
            ptr_arr_stats[num_call_types].count     = 0;
            ptr_arr_stats[num_call_types].ptr_label = CITY_LABEL;
            index_sf = num_call_types;
            num_call_types++;
        }

        // store the three diffs in the SF bucket
        int index_next_open_sf = ptr_arr_stats[index_sf].count;
        ptr_arr_stats[index_sf].ptr_arr_diff_received_to_dispatch[index_next_open_sf] =
            diff_received_to_dispatch;
        ptr_arr_stats[index_sf].ptr_arr_diff_enroute_to_onscene[index_next_open_sf]   =
            diff_enroute_to_onscene;
        ptr_arr_stats[index_sf].ptr_arr_diff_received_to_onscene[index_next_open_sf]  =
            diff_received_to_onscene;
        ptr_arr_stats[index_sf].count++;

        // if subfield matches ptr_subval1 or ptr_subval2, also store there
        if (strcmp(ptr_subfield_val, ptr_subval1) == 0 ||
            strcmp(ptr_subfield_val, ptr_subval2) == 0)
        {
            char *ptr_sub_label = (strcmp(ptr_subfield_val, ptr_subval1) == 0)
                                  ? ptr_subval1 : ptr_subval2;

            int found_sub = 0;
            int index_sub = -1;
            for (index_c = 0; index_c < num_call_types; index_c++)
            {
                if (strcmp(ptr_arr_stats[index_c].ptr_call_type, ptr_call_type) == 0 &&
                    strcmp(ptr_arr_stats[index_c].ptr_label, ptr_sub_label) == 0)
                {
                    found_sub = 1;
                    index_sub = index_c;
                    break;
                }
            }
            if (!found_sub)
            {
                strncpy(ptr_arr_stats[num_call_types].ptr_call_type,
                        ptr_call_type, MAX_NAME_LEN - 1);
                ptr_arr_stats[num_call_types].ptr_call_type[MAX_NAME_LEN - 1] = '\0';
                ptr_arr_stats[num_call_types].count     = 0;
                ptr_arr_stats[num_call_types].ptr_label = ptr_sub_label;
                index_sub = num_call_types;
                num_call_types++;
            }

            // store the three diffs in the subfield bucket
            int index_next_open_sub = ptr_arr_stats[index_sub].count;
            ptr_arr_stats[index_sub].ptr_arr_diff_received_to_dispatch[index_next_open_sub] =
                diff_received_to_dispatch;
            ptr_arr_stats[index_sub].ptr_arr_diff_enroute_to_onscene[index_next_open_sub]   =
                diff_enroute_to_onscene;
            ptr_arr_stats[index_sub].ptr_arr_diff_received_to_onscene[index_next_open_sub]  =
                diff_received_to_onscene;
            ptr_arr_stats[index_sub].count++;
        }
    }

    close(fd_data);

    int total_calls = 0;
    for (index_c = 0; index_c < num_call_types; index_c++)
    {
        if (strcmp(ptr_arr_stats[index_c].ptr_label, CITY_LABEL) == 0)
            total_calls += ptr_arr_stats[index_c].count;
    }

    int unique_sf = 0;
    for (index_c = 0; index_c < num_call_types; index_c++)
    {
        if (strcmp(ptr_arr_stats[index_c].ptr_label, CITY_LABEL) == 0)
            unique_sf++;
    }

    if (DEBUG)
    {
        printf("[DEBUG] %d unique call types found\n",   unique_sf);
        printf("[DEBUG] %d total calls processed (%.1f%%)\n",
               total_calls, (double)total_calls / num_records * 100);
        printf("[DEBUG] %d records skipped (missing data) (%.1f%%)\n\n",
               num_records - total_calls,
               (double)(num_records - total_calls) / num_records * 100);
    }

// section 14 /////////////////////////////////////////////////////////////////

    // print the stats table for each label and each diff
    char *ptr_labels[] = { CITY_LABEL, ptr_subval1, ptr_subval2 };
    int num_labels = 3;
    int index_l;

    for (index_l = 0; index_l < num_labels; index_l++)
    {
        print_stats_table(ptr_arr_stats, num_call_types, ptr_labels[index_l], 0);
        print_stats_table(ptr_arr_stats, num_call_types, ptr_labels[index_l], 1);
        print_stats_table(ptr_arr_stats, num_call_types, ptr_labels[index_l], 2);
    }

// section 15 /////////////////////////////////////////////////////////////////

    // free all malloc memory
    for (index_c = 0; index_c < max_call_types; index_c++)
    {
        free(ptr_arr_stats[index_c].ptr_arr_diff_received_to_dispatch);
        free(ptr_arr_stats[index_c].ptr_arr_diff_enroute_to_onscene);
        free(ptr_arr_stats[index_c].ptr_arr_diff_received_to_onscene);
    }
    free(ptr_arr_stats);

    if (DEBUG) printf("\n[DEBUG] memory freed, return success\n");

// section 16 /////////////////////////////////////////////////////////////////

    return 0;
}
