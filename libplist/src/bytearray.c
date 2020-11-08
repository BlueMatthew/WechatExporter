/*
 * bytearray.c
 * simple byte array implementation
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
#include <string.h>
#include "bytearray.h"

#define PAGE_SIZE 4096

bytearray_t *byte_array_new(size_t initial)
{
	bytearray_t *a = (bytearray_t*)malloc(sizeof(bytearray_t));
	a->capacity = (initial > PAGE_SIZE) ? (initial+(PAGE_SIZE-1)) & (~(PAGE_SIZE-1)) : PAGE_SIZE;
	a->data = malloc(a->capacity);
	a->len = 0;
	return a;
}

void byte_array_free(bytearray_t *ba)
{
	if (!ba) return;
	if (ba->data) {
		free(ba->data);
	}
	free(ba);
}

void byte_array_grow(bytearray_t *ba, size_t amount)
{
	size_t increase = (amount > PAGE_SIZE) ? (amount+(PAGE_SIZE-1)) & (~(PAGE_SIZE-1)) : PAGE_SIZE;
	ba->data = realloc(ba->data, ba->capacity + increase);
	ba->capacity += increase;
}

void byte_array_append(bytearray_t *ba, void *buf, size_t len)
{
	if (!ba || !ba->data || (len <= 0)) return;
	size_t remaining = ba->capacity-ba->len;
	if (len > remaining) {
		size_t needed = len - remaining;
		byte_array_grow(ba, needed);
	}
	memcpy(((char*)ba->data) + ba->len, buf, len);
	ba->len += len;
}
