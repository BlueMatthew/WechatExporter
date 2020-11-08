/*
 * bytearray.h
 * header file for simple byte array implementation
 *
 * Copyright (c) 2011 Nikias Bassen, All Rights Reserved.
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
#ifndef BYTEARRAY_H
#define BYTEARRAY_H
#include <stdlib.h>

typedef struct bytearray_t {
	void *data;
	size_t len;
	size_t capacity;
} bytearray_t;

bytearray_t *byte_array_new(size_t initial);
void byte_array_free(bytearray_t *ba);
void byte_array_grow(bytearray_t *ba, size_t amount);
void byte_array_append(bytearray_t *ba, void *buf, size_t len);

#endif
