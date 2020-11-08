/*
 * strbuf.h
 * header file for simple string buffer, using the bytearray as underlying
 * structure.
 *
 * Copyright (c) 2016 Nikias Bassen, All Rights Reserved.
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
#ifndef STRBUF_H
#define STRBUF_H
#include <stdlib.h>
#include "bytearray.h"

typedef struct bytearray_t strbuf_t;

#define str_buf_new(__sz) byte_array_new(__sz)
#define str_buf_free(__ba) byte_array_free(__ba)
#define str_buf_grow(__ba, __am) byte_array_grow(__ba, __am)
#define str_buf_append(__ba, __str, __len) byte_array_append(__ba, (void*)(__str), __len)

#endif
