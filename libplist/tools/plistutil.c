/*
 * plistutil.c
 * Simple tool to convert a plist into different formats
 *
 * Copyright (c) 2009-2020 Martin Szulecki All Rights Reserved.
 * Copyright (c) 2013-2020 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2008 Zach C., All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "plist/plist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#define BUF_SIZE 2048 // Seems to be a decent start to cover most stdin files

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

typedef struct _options
{
    char *in_file, *out_file;
    uint8_t debug, in_fmt, out_fmt; // fmts 0 = undef, 1 = bin, 2 = xml, 3 = json someday
} options_t;

static void print_usage(int argc, char *argv[])
{
    char *name = NULL;
    name = strrchr(argv[0], '/');
    printf("Usage: %s [OPTIONS] [-i FILE] [-o FILE]\n", (name ? name + 1: argv[0]));
    printf("\n");
    printf("Convert a plist FILE from binary to XML format or vice-versa.\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -i, --infile FILE       Optional FILE to convert from or stdin if - or not used\n");
    printf("  -o, --outfile FILE      Optional FILE to convert to or stdout if - or not used\n");
    printf("  -f, --format [bin|xml]  Force output format, regardless of input type\n");
    printf("  -d, --debug             Enable extended debug output\n");
    printf("  -v, --version           Print version information\n");
    printf("\n");
    printf("Homepage:    <" PACKAGE_URL ">\n");
    printf("Bug Reports: <" PACKAGE_BUGREPORT ">\n");
}

static options_t *parse_arguments(int argc, char *argv[])
{
    int i = 0;

    options_t *options = (options_t *) malloc(sizeof(options_t));
    memset(options, 0, sizeof(options_t));
    options->out_fmt = 0;

    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--infile") || !strcmp(argv[i], "-i"))
        {
            if ((i + 1) == argc)
            {
                free(options);
                return NULL;
            }
            options->in_file = argv[i + 1];
            i++;
            continue;
        }
        else if (!strcmp(argv[i], "--outfile") || !strcmp(argv[i], "-o"))
        {
            if ((i + 1) == argc)
            {
                free(options);
                return NULL;
            }
            options->out_file = argv[i + 1];
            i++;
            continue;
        }
        else if (!strcmp(argv[i], "--format") || !strcmp(argv[i], "-f"))
        {
            if ((i + 1) == argc)
            {
                free(options);
                return NULL;
            }
            if (!strncmp(argv[i+1], "bin", 3)) {
                options->out_fmt = 1;
            } else if (!strncmp(argv[i+1], "xml", 3)) {
                options->out_fmt = 2;
            } else {
                printf("ERROR: Unsupported output format\n");
                free(options);
                return NULL;
            }
            i++;
            continue;
        }
        else if (!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d"))
        {
            options->debug = 1;
        }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
        {
            free(options);
            return NULL;
        }
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v"))
        {
            printf("plistutil %s\n", PACKAGE_VERSION);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: Invalid option '%s'\n", argv[i]);
            free(options);
            return NULL;
        }
    }

    return options;
}

int main(int argc, char *argv[])
{
    FILE *iplist = NULL;
    plist_t root_node = NULL;
    char *plist_out = NULL;
    uint32_t size = 0;
    int read_size = 0;
    char *plist_entire = NULL;
    struct stat filestats;
    options_t *options = parse_arguments(argc, argv);

    if (!options)
    {
        print_usage(argc, argv);
        return 0;
    }

    if (!options->in_file || !strcmp(options->in_file, "-"))
    {
#ifndef _MSC_VER
        read_size = 0;
        plist_entire = malloc(sizeof(char) * BUF_SIZE);
        if(plist_entire == NULL)
        {
            printf("ERROR: Failed to allocate buffer to read from stdin");
            free(options);
            return 1;
        }
        plist_entire[read_size] = '\0';
        char ch;
        while(read(STDIN_FILENO, &ch, 1) > 0)
        {
            if (read_size >= BUF_SIZE) {
                char *old = plist_entire;
                plist_entire = realloc(plist_entire, sizeof(char) * (read_size + 1));
                if (plist_entire == NULL)
                {
                    printf("ERROR: Failed to reallocate stdin buffer\n");
                    free(old);
                    free(options);
                    return 1;
                }
            }
            plist_entire[read_size] = ch;
            read_size++;
        }
        plist_entire[read_size] = '\0';

        // Not positive we need this, but it doesnt seem to hurt lol
        if(ferror(stdin))
        {
            printf("ERROR: reading from stdin.\n");
            free(plist_entire);
            free(options);
            return 1;
        }

        if (read_size < 8) {
            printf("ERROR: Input file is too small to contain valid plist data.\n");
            free(plist_entire);
            free(options);
            return 1;
        }
#else
        printf("ERROR: reading from stdin is not supported on Windows");
        return -1;
#endif
    }
    else
    {
        // read input file
        iplist = fopen(options->in_file, "rb");
        if (!iplist) {
            printf("ERROR: Could not open input file '%s': %s\n", options->in_file, strerror(errno));
            free(options);
            return 1;
        }

        memset(&filestats, '\0', sizeof(struct stat));
        fstat(fileno(iplist), &filestats);

        if (filestats.st_size < 8) {
            printf("ERROR: Input file is too small to contain valid plist data.\n");
            free(options);
            fclose(iplist);
            return -1;
        }

        plist_entire = (char *) malloc(sizeof(char) * (filestats.st_size + 1));
        read_size = fread(plist_entire, sizeof(char), filestats.st_size, iplist);
        fclose(iplist);
    }

    if (options->out_fmt == 0) {
        // convert from binary to xml or vice-versa<br>
        if (plist_is_binary(plist_entire, read_size))
        {
            plist_from_bin(plist_entire, read_size, &root_node);
            plist_to_xml(root_node, &plist_out, &size);
        }
        else
        {
            plist_from_xml(plist_entire, read_size, &root_node);
            plist_to_bin(root_node, &plist_out, &size);
        }
    }
    else
    {
        if (options->out_fmt == 1) {
            if (plist_is_binary(plist_entire, read_size))
            {
                plist_out = malloc(sizeof(char) * read_size);
                memcpy(plist_out, plist_entire, read_size);
                size = read_size;
            }
            else
            {
                plist_from_xml(plist_entire, read_size, &root_node);
                plist_to_bin(root_node, &plist_out, &size);
            }
        } else if (options->out_fmt == 2) {
            if (plist_is_binary(plist_entire, read_size)) {
                plist_from_bin(plist_entire, read_size, &root_node);
                plist_to_xml(root_node, &plist_out, &size);
            }
            else
            {
                plist_out = malloc(sizeof(char) * read_size);
                memcpy(plist_out, plist_entire, read_size);
                size = read_size;
            }
        }
    }
    plist_free(root_node);
    free(plist_entire);

    if (plist_out)
    {
        if (options->out_file != NULL && strcmp(options->out_file, "-"))
        {
            FILE *oplist = fopen(options->out_file, "wb");
            if (!oplist) {
                printf("ERROR: Could not open output file '%s': %s\n", options->out_file, strerror(errno));
                free(options);
                return 1;
            }
            fwrite(plist_out, size, sizeof(char), oplist);
            fclose(oplist);
        }
        // if no output file specified, write to stdout
        else
            fwrite(plist_out, size, sizeof(char), stdout);

        free(plist_out);
    }
    else
        printf("ERROR: Failed to convert input file.\n");

    free(options);
    return 0;
}
