/*
 * bplist.c
 * Binary plist implementation
 *
 * Copyright (c) 2011-2017 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2008-2010 Jonathan Beck, All Rights Reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <ctype.h>
#include <inttypes.h>

#include <plist/plist.h>
#include "plist.h"
#include "hashtable.h"
#include "bytearray.h"
#include "ptrarray.h"

#include <node.h>

#ifndef _MSC_VER
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#else
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

/* Magic marker and size. */
#define BPLIST_MAGIC            ((uint8_t*)"bplist")
#define BPLIST_MAGIC_SIZE       6

#define BPLIST_VERSION          ((uint8_t*)"00")
#define BPLIST_VERSION_SIZE     2

PACK(typedef struct {
    uint8_t unused[6];
    uint8_t offset_size;
    uint8_t ref_size;
    uint64_t num_objects;
    uint64_t root_object_index;
    uint64_t offset_table_offset;
}) bplist_trailer_t;

enum
{
    BPLIST_NULL = 0x00,
    BPLIST_FALSE = 0x08,
    BPLIST_TRUE = 0x09,
    BPLIST_FILL = 0x0F,			/* will be used for length grabbing */
    BPLIST_UINT = 0x10,
    BPLIST_REAL = 0x20,
    BPLIST_DATE = 0x30,
    BPLIST_DATA = 0x40,
    BPLIST_STRING = 0x50,
    BPLIST_UNICODE = 0x60,
    BPLIST_UNK_0x70 = 0x70,
    BPLIST_UID = 0x80,
    BPLIST_ARRAY = 0xA0,
    BPLIST_SET = 0xC0,
    BPLIST_DICT = 0xD0,
    BPLIST_MASK = 0xF0
};

union plist_uint_ptr
{
    const void *src;
    uint8_t *u8ptr;
    uint16_t *u16ptr;
    uint32_t *u32ptr;
    uint64_t *u64ptr;
};

#ifdef _MSC_VER
uint64_t get_unaligned_64(uint64_t *ptr)
{
	uint64_t temp;
	memcpy(&temp, ptr, sizeof(temp));
	return temp;
}

uint32_t get_unaligned_32(uint32_t *ptr)
{
	uint32_t temp;
	memcpy(&temp, ptr, sizeof(temp));
	return temp;
}

uint16_t get_unaligned_16(uint16_t *ptr)
{
	uint16_t temp;
	memcpy(&temp, ptr, sizeof(temp));
	return temp;
}
#else
#define get_unaligned(ptr)			  \
  ({                                              \
    struct __attribute__((packed)) {		  \
      typeof(*(ptr)) __v;			  \
    } *__p = (void *) (ptr);			  \
    __p->__v;					  \
  })
#endif

#ifndef bswap16
#define bswap16(x)   ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#endif

#ifndef bswap32
#define bswap32(x)   ((((x) & 0xFF000000) >> 24) \
                    | (((x) & 0x00FF0000) >>  8) \
                    | (((x) & 0x0000FF00) <<  8) \
                    | (((x) & 0x000000FF) << 24))
#endif

#ifndef bswap64
#define bswap64(x)   ((((x) & 0xFF00000000000000ull) >> 56) \
                    | (((x) & 0x00FF000000000000ull) >> 40) \
                    | (((x) & 0x0000FF0000000000ull) >> 24) \
                    | (((x) & 0x000000FF00000000ull) >>  8) \
                    | (((x) & 0x00000000FF000000ull) <<  8) \
                    | (((x) & 0x0000000000FF0000ull) << 24) \
                    | (((x) & 0x000000000000FF00ull) << 40) \
                    | (((x) & 0x00000000000000FFull) << 56))
#endif

#ifndef be16toh
#ifdef __BIG_ENDIAN__
#define be16toh(x) (x)
#else
#define be16toh(x) bswap16(x)
#endif
#endif

#ifndef be32toh
#ifdef __BIG_ENDIAN__
#define be32toh(x) (x)
#else
#define be32toh(x) bswap32(x)
#endif
#endif

#ifndef be64toh
#ifdef __BIG_ENDIAN__
#define be64toh(x) (x)
#else
#define be64toh(x) bswap64(x)
#endif
#endif

#ifdef __BIG_ENDIAN__
#define beNtoh(x,n) (x >> ((8-n) << 3))
#else
#define beNtoh(x,n) be64toh(x << ((8-n) << 3))
#endif

#ifdef _MSC_VER
uint64_t UINT_TO_HOST(void* x, uint8_t n)
{
		union plist_uint_ptr __up;
		// Adding to void* is a GCC extension, see http://gcc.gnu.org/onlinedocs/gcc-4.8.0/gcc/Pointer-Arith.html
		__up.src = (n > 8) ? (char *)x + (n - 8) : x;
		return (n >= 8 ? be64toh( get_unaligned_64(__up.u64ptr) ) :
		(n == 4 ? be32toh( get_unaligned_32(__up.u32ptr) ) :
		(n == 2 ? be16toh( get_unaligned_16(__up.u16ptr) ) :
                (n == 1 ? *__up.u8ptr :
		beNtoh( get_unaligned_64(__up.u64ptr), n)
		))));
}
#else
#define UINT_TO_HOST(x, n) \
	({ \
		union plist_uint_ptr __up; \
		__up.src = (n > 8) ? x + (n - 8) : x; \
		(n >= 8 ? be64toh( get_unaligned(__up.u64ptr) ) : \
		(n == 4 ? be32toh( get_unaligned(__up.u32ptr) ) : \
		(n == 2 ? be16toh( get_unaligned(__up.u16ptr) ) : \
                (n == 1 ? *__up.u8ptr : \
		beNtoh( get_unaligned(__up.u64ptr), n) \
		)))); \
	})
#endif

#define get_needed_bytes(x) \
		( ((uint64_t)x) < (1ULL << 8) ? 1 : \
		( ((uint64_t)x) < (1ULL << 16) ? 2 : \
		( ((uint64_t)x) < (1ULL << 24) ? 3 : \
		( ((uint64_t)x) < (1ULL << 32) ? 4 : 8))))

#define get_real_bytes(x) (x == (float) x ? sizeof(float) : sizeof(double))

#if (defined(__LITTLE_ENDIAN__) \
     && !defined(__FLOAT_WORD_ORDER__)) \
 || (defined(__FLOAT_WORD_ORDER__) \
     && __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__) \
 || (defined(_MSC_VER) && (REG_DWORD == REG_DWORD_LITTLE_ENDIAN))
#define float_bswap64(x) bswap64(x)
#define float_bswap32(x) bswap32(x)
#else
#define float_bswap64(x) (x)
#define float_bswap32(x) (x)
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_umulll_overflow) || __GNUC__ >= 5
#define uint64_mul_overflow(a, b, r) __builtin_umulll_overflow(a, b, (unsigned long long*)r)
#else
static int uint64_mul_overflow(uint64_t a, uint64_t b, uint64_t *res)
{
    *res = a * b;
    return (a > UINT64_MAX / b);
}
#endif

#define NODE_IS_ROOT(x) (((node_t*)x)->isRoot)

struct bplist_data {
    const char* data;
    uint64_t size;
    uint64_t num_objects;
    uint8_t ref_size;
    uint8_t offset_size;
    const char* offset_table;
    uint32_t level;
    ptrarray_t* used_indexes;
};

#ifdef DEBUG
static int plist_bin_debug = 0;
#define PLIST_BIN_ERR(...) if (plist_bin_debug) { fprintf(stderr, "libplist[binparser] ERROR: " __VA_ARGS__); }
#else
#define PLIST_BIN_ERR(...)
#endif

void plist_bin_init(void)
{
    /* init binary plist stuff */
#ifdef DEBUG
    char *env_debug = getenv("PLIST_BIN_DEBUG");
    if (env_debug && !strcmp(env_debug, "1")) {
        plist_bin_debug = 1;
    }
#endif
}

void plist_bin_deinit(void)
{
    /* deinit binary plist stuff */
}

static plist_t parse_bin_node_at_index(struct bplist_data *bplist, uint32_t node_index);

static plist_t parse_uint_node(const char **bnode, uint8_t size)
{
    plist_data_t data = plist_new_plist_data();

    size = 1 << size;			// make length less misleading
    switch (size)
    {
    case sizeof(uint8_t):
    case sizeof(uint16_t):
    case sizeof(uint32_t):
    case sizeof(uint64_t):
        data->length = sizeof(uint64_t);
        break;
    case 16:
        data->length = size;
        break;
    default:
        free(data);
        PLIST_BIN_ERR("%s: Invalid byte size for integer node\n", __func__);
        return NULL;
    };

    data->intval = UINT_TO_HOST((void*)*bnode, size);

    (*bnode) += size;
    data->type = PLIST_UINT;

    return node_create(NULL, data);
}

static plist_t parse_real_node(const char **bnode, uint8_t size)
{
    plist_data_t data = plist_new_plist_data();
    uint8_t buf[8];

    size = 1 << size;			// make length less misleading
    switch (size)
    {
    case sizeof(uint32_t):
		
#ifdef _MSC_VER
		*(uint32_t*)buf = float_bswap32(get_unaligned_32((uint32_t*)*bnode));
#else
		*(uint32_t*)buf = float_bswap32(get_unaligned((uint32_t*)*bnode));
#endif
        data->realval = *(float *) buf;
        break;
    case sizeof(uint64_t):
#ifdef _MSC_VER
        *(uint64_t*)buf = float_bswap64(get_unaligned_64((uint64_t*)*bnode));
#else
		*(uint64_t*)buf = float_bswap64(get_unaligned((uint64_t*)*bnode));
#endif
        data->realval = *(double *) buf;
        break;
    default:
        free(data);
        PLIST_BIN_ERR("%s: Invalid byte size for real node\n", __func__);
        return NULL;
    }
    data->type = PLIST_REAL;
    data->length = sizeof(double);

    return node_create(NULL, data);
}

static plist_t parse_date_node(const char **bnode, uint8_t size)
{
    plist_t node = parse_real_node(bnode, size);
    plist_data_t data = plist_get_data(node);

    data->type = PLIST_DATE;

    return node;
}

static plist_t parse_string_node(const char **bnode, uint64_t size)
{
    plist_data_t data = plist_new_plist_data();

    data->type = PLIST_STRING;
    data->strval = (char *) malloc(sizeof(char) * (size + 1));
    if (!data->strval) {
        plist_free_data(data);
        PLIST_BIN_ERR("%s: Could not allocate %" PRIu64 " bytes\n", __func__, sizeof(char) * (size + 1));
        return NULL;
    }
    memcpy(data->strval, *bnode, size);
    data->strval[size] = '\0';
    data->length = strlen(data->strval);

    return node_create(NULL, data);
}

static char *plist_utf16be_to_utf8(uint16_t *unistr, long len, long *items_read, long *items_written)
{
	if (!unistr || (len <= 0)) return NULL;
	char *outbuf;
	int p = 0;
	long i = 0;

	uint16_t wc;
	uint32_t w;
	int read_lead_surrogate = 0;

	outbuf = (char*)malloc(4*(len+1));
	if (!outbuf) {
		PLIST_BIN_ERR("%s: Could not allocate %" PRIu64 " bytes\n", __func__, (uint64_t)(4*(len+1)));
		return NULL;
	}

	while (i < len) {
#if _MSC_VER
		wc = be16toh(get_unaligned_16(unistr + i));
#else
		wc = be16toh(get_unaligned(unistr + i));
#endif
		i++;
		if (wc >= 0xD800 && wc <= 0xDBFF) {
			if (!read_lead_surrogate) {
				read_lead_surrogate = 1;
				w = 0x010000 + ((wc & 0x3FF) << 10);
			} else {
				// This is invalid, the next 16 bit char should be a trail surrogate.
				// Handling error by skipping.
				read_lead_surrogate = 0;
			}
		} else if (wc >= 0xDC00 && wc <= 0xDFFF) {
			if (read_lead_surrogate) {
				read_lead_surrogate = 0;
				w = w | (wc & 0x3FF);
				outbuf[p++] = (char)(0xF0 + ((w >> 18) & 0x7));
				outbuf[p++] = (char)(0x80 + ((w >> 12) & 0x3F));
				outbuf[p++] = (char)(0x80 + ((w >> 6) & 0x3F));
				outbuf[p++] = (char)(0x80 + (w & 0x3F));
			} else {
				// This is invalid.  A trail surrogate should always follow a lead surrogate.
				// Handling error by skipping
			}
		} else if (wc >= 0x800) {
			outbuf[p++] = (char)(0xE0 + ((wc >> 12) & 0xF));
			outbuf[p++] = (char)(0x80 + ((wc >> 6) & 0x3F));
			outbuf[p++] = (char)(0x80 + (wc & 0x3F));
		} else if (wc >= 0x80) {
			outbuf[p++] = (char)(0xC0 + ((wc >> 6) & 0x1F));
			outbuf[p++] = (char)(0x80 + (wc & 0x3F));
		} else {
			outbuf[p++] = (char)(wc & 0x7F);
		}
	}
	if (items_read) {
		*items_read = i;
	}
	if (items_written) {
		*items_written = p;
	}
	outbuf[p] = 0;

	return outbuf;
}

static plist_t parse_unicode_node(const char **bnode, uint64_t size)
{
    plist_data_t data = plist_new_plist_data();
    char *tmpstr = NULL;
    long items_read = 0;
    long items_written = 0;

    data->type = PLIST_STRING;

    tmpstr = plist_utf16be_to_utf8((uint16_t*)(*bnode), size, &items_read, &items_written);
    if (!tmpstr) {
        plist_free_data(data);
        return NULL;
    }
    tmpstr[items_written] = '\0';

    data->type = PLIST_STRING;
    data->strval = (char*)realloc(tmpstr, items_written+1);
    if (!data->strval)
        data->strval = tmpstr;
    data->length = items_written;
    return node_create(NULL, data);
}

static plist_t parse_data_node(const char **bnode, uint64_t size)
{
    plist_data_t data = plist_new_plist_data();

    data->type = PLIST_DATA;
    data->length = size;
    data->buff = (uint8_t *) malloc(sizeof(uint8_t) * size);
    if (!data->strval) {
        plist_free_data(data);
        PLIST_BIN_ERR("%s: Could not allocate %" PRIu64 " bytes\n", __func__, sizeof(uint8_t) * size);
        return NULL;
    }
    memcpy(data->buff, *bnode, sizeof(uint8_t) * size);

    return node_create(NULL, data);
}

static plist_t parse_dict_node(struct bplist_data *bplist, const char** bnode, uint64_t size)
{
    uint64_t j;
    uint64_t str_i = 0, str_j = 0;
    uint64_t index1, index2;
    plist_data_t data = plist_new_plist_data();
    const char *index1_ptr = NULL;
    const char *index2_ptr = NULL;

    data->type = PLIST_DICT;
    data->length = size;

    plist_t node = node_create(NULL, data);

    for (j = 0; j < data->length; j++) {
        str_i = j * bplist->ref_size;
        str_j = (j + size) * bplist->ref_size;
        index1_ptr = (*bnode) + str_i;
        index2_ptr = (*bnode) + str_j;

        if ((index1_ptr < bplist->data || index1_ptr + bplist->ref_size > bplist->offset_table) ||
            (index2_ptr < bplist->data || index2_ptr + bplist->ref_size > bplist->offset_table)) {
            plist_free(node);
            PLIST_BIN_ERR("%s: dict entry %" PRIu64 " is outside of valid range\n", __func__, j);
            return NULL;
        }

        index1 = UINT_TO_HOST((void*)index1_ptr, bplist->ref_size);
        index2 = UINT_TO_HOST((void*)index2_ptr, bplist->ref_size);

        if (index1 >= bplist->num_objects) {
            plist_free(node);
            PLIST_BIN_ERR("%s: dict entry %" PRIu64 ": key index (%" PRIu64 ") must be smaller than the number of objects (%" PRIu64 ")\n", __func__, j, index1, bplist->num_objects);
            return NULL;
        }
        if (index2 >= bplist->num_objects) {
            plist_free(node);
            PLIST_BIN_ERR("%s: dict entry %" PRIu64 ": value index (%" PRIu64 ") must be smaller than the number of objects (%" PRIu64 ")\n", __func__, j, index1, bplist->num_objects);
            return NULL;
        }

        /* process key node */
        plist_t key = parse_bin_node_at_index(bplist, index1);
        if (!key) {
            plist_free(node);
            return NULL;
        }

        if (plist_get_data(key)->type != PLIST_STRING) {
            PLIST_BIN_ERR("%s: dict entry %" PRIu64 ": invalid node type for key\n", __func__, j);
            plist_free(key);
            plist_free(node);
            return NULL;
        }

        /* enforce key type */
        plist_get_data(key)->type = PLIST_KEY;
        if (!plist_get_data(key)->strval) {
            PLIST_BIN_ERR("%s: dict entry %" PRIu64 ": key must not be NULL\n", __func__, j);
            plist_free(key);
            plist_free(node);
            return NULL;
        }

        /* process value node */
        plist_t val = parse_bin_node_at_index(bplist, index2);
        if (!val) {
            plist_free(key);
            plist_free(node);
            return NULL;
        }

        node_attach((node_t *)node, (node_t *)key);
        node_attach((node_t *)node, (node_t *)val);
    }

    return node;
}

static plist_t parse_array_node(struct bplist_data *bplist, const char** bnode, uint64_t size)
{
    uint64_t j;
    uint64_t str_j = 0;
    uint64_t index1;
    plist_data_t data = plist_new_plist_data();
    const char *index1_ptr = NULL;

    data->type = PLIST_ARRAY;
    data->length = size;

    plist_t node = node_create(NULL, data);

    for (j = 0; j < data->length; j++) {
        str_j = j * bplist->ref_size;
        index1_ptr = (*bnode) + str_j;

        if (index1_ptr < bplist->data || index1_ptr + bplist->ref_size > bplist->offset_table) {
            plist_free(node);
            PLIST_BIN_ERR("%s: array item %" PRIu64 " is outside of valid range\n", __func__, j);
            return NULL;
        }

        index1 = UINT_TO_HOST((void*)index1_ptr, bplist->ref_size);

        if (index1 >= bplist->num_objects) {
            plist_free(node);
            PLIST_BIN_ERR("%s: array item %" PRIu64 " object index (%" PRIu64 ") must be smaller than the number of objects (%" PRIu64 ")\n", __func__, j, index1, bplist->num_objects);
            return NULL;
        }

        /* process value node */
        plist_t val = parse_bin_node_at_index(bplist, index1);
        if (!val) {
            plist_free(node);
            return NULL;
        }

        node_attach((node_t *)node, (node_t *)val);
    }

    return node;
}

static plist_t parse_uid_node(const char **bnode, uint8_t size)
{
    plist_data_t data = plist_new_plist_data();
    size = size + 1;
    data->intval = UINT_TO_HOST((void*)*bnode, size);
    if (data->intval > UINT32_MAX) {
        PLIST_BIN_ERR("%s: value %" PRIu64 " too large for UID node (must be <= %u)\n", __func__, (uint64_t)data->intval, UINT32_MAX);
        free(data);
        return NULL;
    }

    (*bnode) += size;
    data->type = PLIST_UID;
    data->length = sizeof(uint64_t);

    return node_create(NULL, data);
}

static plist_t parse_bin_node(struct bplist_data *bplist, const char** object)
{
    uint16_t type = 0;
    uint64_t size = 0;
    uint64_t pobject = 0;
    uint64_t poffset_table = (uint64_t)(uintptr_t)bplist->offset_table;

    if (!object)
        return NULL;

    type = (**object) & BPLIST_MASK;
    size = (**object) & BPLIST_FILL;
    (*object)++;

    if (size == BPLIST_FILL) {
        switch (type) {
        case BPLIST_DATA:
        case BPLIST_STRING:
        case BPLIST_UNICODE:
        case BPLIST_ARRAY:
        case BPLIST_SET:
        case BPLIST_DICT:
        {
            uint16_t next_size = **object & BPLIST_FILL;
            if ((**object & BPLIST_MASK) != BPLIST_UINT) {
                PLIST_BIN_ERR("%s: invalid size node type for node type 0x%02x: found 0x%02x, expected 0x%02x\n", __func__, type, **object & BPLIST_MASK, BPLIST_UINT);
                return NULL;
            }
            (*object)++;
            next_size = 1 << next_size;
            if (*object + next_size > bplist->offset_table) {
                PLIST_BIN_ERR("%s: size node data bytes for node type 0x%02x point outside of valid range\n", __func__, type);
                return NULL;
            }
            size = UINT_TO_HOST((void*)*object, next_size);
            (*object) += next_size;
            break;
        }
        default:
            break;
        }
    }

    pobject = (uint64_t)(uintptr_t)*object;

    switch (type)
    {

    case BPLIST_NULL:
        switch (size)
        {

        case BPLIST_TRUE:
        {
            plist_data_t data = plist_new_plist_data();
            data->type = PLIST_BOOLEAN;
            data->boolval = TRUE;
            data->length = 1;
            return node_create(NULL, data);
        }

        case BPLIST_FALSE:
        {
            plist_data_t data = plist_new_plist_data();
            data->type = PLIST_BOOLEAN;
            data->boolval = FALSE;
            data->length = 1;
            return node_create(NULL, data);
        }

        case BPLIST_NULL:
        default:
            return NULL;
        }

    case BPLIST_UINT:
        if (pobject + (uint64_t)(1 << size) > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_UINT data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_uint_node(object, size);

    case BPLIST_REAL:
        if (pobject + (uint64_t)(1 << size) > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_REAL data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_real_node(object, size);

    case BPLIST_DATE:
        if (3 != size) {
            PLIST_BIN_ERR("%s: invalid data size for BPLIST_DATE node\n", __func__);
            return NULL;
        }
        if (pobject + (uint64_t)(1 << size) > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_DATE data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_date_node(object, size);

    case BPLIST_DATA:
        if (pobject + size < pobject || pobject + size > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_DATA data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_data_node(object, size);

    case BPLIST_STRING:
        if (pobject + size < pobject || pobject + size > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_STRING data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_string_node(object, size);

    case BPLIST_UNICODE:
        if (size*2 < size) {
            PLIST_BIN_ERR("%s: Integer overflow when calculating BPLIST_UNICODE data size.\n", __func__);
            return NULL;
        }
        if (pobject + size*2 < pobject || pobject + size*2 > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_UNICODE data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_unicode_node(object, size);

    case BPLIST_SET:
    case BPLIST_ARRAY:
        if (pobject + size < pobject || pobject + size > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_ARRAY data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_array_node(bplist, object, size);

    case BPLIST_UID:
        if (pobject + size+1 > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_UID data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_uid_node(object, size);

    case BPLIST_DICT:
        if (pobject + size < pobject || pobject + size > poffset_table) {
            PLIST_BIN_ERR("%s: BPLIST_DICT data bytes point outside of valid range\n", __func__);
            return NULL;
        }
        return parse_dict_node(bplist, object, size);

    default:
        PLIST_BIN_ERR("%s: unexpected node type 0x%02x\n", __func__, type);
        return NULL;
    }
    return NULL;
}

static plist_t parse_bin_node_at_index(struct bplist_data *bplist, uint32_t node_index)
{
    int i = 0;
    const char* ptr = NULL;
    plist_t plist = NULL;
    const char* idx_ptr = NULL;

    if (node_index >= bplist->num_objects) {
        PLIST_BIN_ERR("node index (%u) must be smaller than the number of objects (%" PRIu64 ")\n", node_index, bplist->num_objects);
        return NULL;
    }

    idx_ptr = bplist->offset_table + node_index * bplist->offset_size;
    if (idx_ptr < bplist->offset_table ||
        idx_ptr >= bplist->offset_table + bplist->num_objects * bplist->offset_size) {
        PLIST_BIN_ERR("node index %u points outside of valid range\n", node_index);
        return NULL;
    }

    ptr = bplist->data + UINT_TO_HOST((void*)idx_ptr, bplist->offset_size);
    /* make sure the node offset is in a sane range */
    if ((ptr < bplist->data) || (ptr >= bplist->offset_table)) {
        PLIST_BIN_ERR("offset for node index %u points outside of valid range\n", node_index);
        return NULL;
    }

    /* store node_index for current recursion level */
    if ((uint32_t)ptr_array_size(bplist->used_indexes) < bplist->level+1) {
        while ((uint32_t)ptr_array_size(bplist->used_indexes) < bplist->level+1) {
            ptr_array_add(bplist->used_indexes, (void*)(uintptr_t)node_index);
        }
    } else {
	ptr_array_set(bplist->used_indexes, (void*)(uintptr_t)node_index, bplist->level);
    }

    /* recursion check */
    if (bplist->level > 0) {
        for (i = bplist->level-1; i >= 0; i--) {
            uint32_t node_i = (uint32_t)(uintptr_t)ptr_array_index(bplist->used_indexes, i);
            uint32_t node_level = (uint32_t)(uintptr_t)ptr_array_index(bplist->used_indexes, bplist->level);
            if (node_i == node_level) {
                PLIST_BIN_ERR("recursion detected in binary plist\n");
                return NULL;
            }
        }
    }

    /* finally parse node */
    bplist->level++;
    plist = parse_bin_node(bplist, &ptr);
    bplist->level--;
    return plist;
}

PLIST_API void plist_from_bin(const char *plist_bin, uint32_t length, plist_t * plist)
{
    bplist_trailer_t *trailer = NULL;
    uint8_t offset_size = 0;
    uint8_t ref_size = 0;
    uint64_t num_objects = 0;
    uint64_t root_object = 0;
    const char *offset_table = NULL;
    uint64_t offset_table_size = 0;
    const char *start_data = NULL;
    const char *end_data = NULL;

    //first check we have enough data
    if (!(length >= BPLIST_MAGIC_SIZE + BPLIST_VERSION_SIZE + sizeof(bplist_trailer_t))) {
        PLIST_BIN_ERR("plist data is to small to hold a binary plist\n");
        return;
    }
    //check that plist_bin in actually a plist
    if (memcmp(plist_bin, BPLIST_MAGIC, BPLIST_MAGIC_SIZE) != 0) {
        PLIST_BIN_ERR("bplist magic mismatch\n");
        return;
    }
    //check for known version
    if (memcmp(plist_bin + BPLIST_MAGIC_SIZE, BPLIST_VERSION, BPLIST_VERSION_SIZE) != 0) {
        PLIST_BIN_ERR("unsupported binary plist version '%.2s\n", plist_bin+BPLIST_MAGIC_SIZE);
        return;
    }

    start_data = plist_bin + BPLIST_MAGIC_SIZE + BPLIST_VERSION_SIZE;
    end_data = plist_bin + length - sizeof(bplist_trailer_t);

    //now parse trailer
    trailer = (bplist_trailer_t*)end_data;

    offset_size = trailer->offset_size;
    ref_size = trailer->ref_size;
    num_objects = be64toh(trailer->num_objects);
    root_object = be64toh(trailer->root_object_index);
    offset_table = (char *)(plist_bin + be64toh(trailer->offset_table_offset));

    if (num_objects == 0) {
        PLIST_BIN_ERR("number of objects must be larger than 0\n");
        return;
    }

    if (offset_size == 0) {
        PLIST_BIN_ERR("offset size in trailer must be larger than 0\n");
        return;
    }

    if (ref_size == 0) {
        PLIST_BIN_ERR("object reference size in trailer must be larger than 0\n");
        return;
    }

    if (root_object >= num_objects) {
        PLIST_BIN_ERR("root object index (%" PRIu64 ") must be smaller than number of objects (%" PRIu64 ")\n", root_object, num_objects);
        return;
    }

    if (offset_table < start_data || offset_table >= end_data) {
        PLIST_BIN_ERR("offset table offset points outside of valid range\n");
        return;
    }

    if (uint64_mul_overflow(num_objects, offset_size, &offset_table_size)) {
        PLIST_BIN_ERR("integer overflow when calculating offset table size\n");
        return;
    }

    if ((offset_table + offset_table_size < offset_table) || (offset_table + offset_table_size > end_data)) {
        PLIST_BIN_ERR("offset table points outside of valid range\n");
        return;
    }

    struct bplist_data bplist;
    bplist.data = plist_bin;
    bplist.size = length;
    bplist.num_objects = num_objects;
    bplist.ref_size = ref_size;
    bplist.offset_size = offset_size;
    bplist.offset_table = offset_table;
    bplist.level = 0;
    bplist.used_indexes = ptr_array_new(16);

    if (!bplist.used_indexes) {
        PLIST_BIN_ERR("failed to create array to hold used node indexes. Out of memory?\n");
        return;
    }

    *plist = parse_bin_node_at_index(&bplist, root_object);

    ptr_array_free(bplist.used_indexes);
}

static unsigned int plist_data_hash(const void* key)
{
    plist_data_t data = plist_get_data((plist_t) key);

    unsigned int hash = data->type;
    unsigned int i = 0;

    char *buff = NULL;
    unsigned int size = 0;

    switch (data->type)
    {
    case PLIST_BOOLEAN:
    case PLIST_UINT:
    case PLIST_REAL:
    case PLIST_DATE:
    case PLIST_UID:
        buff = (char *) &data->intval;	//works also for real as we use an union
        size = 8;
        break;
    case PLIST_KEY:
    case PLIST_STRING:
        buff = data->strval;
        size = data->length;
        break;
    case PLIST_DATA:
    case PLIST_ARRAY:
    case PLIST_DICT:
        //for these types only hash pointer
        buff = (char *) &key;
        size = sizeof(const void*);
        break;
    default:
        break;
    }

    // now perform hash using djb2 hashing algorithm
    // see: http://www.cse.yorku.ca/~oz/hash.html
    hash += 5381;
    for (i = 0; i < size; buff++, i++) {
        hash = ((hash << 5) + hash) + *buff;
    }

    return hash;
}

struct serialize_s
{
    ptrarray_t* objects;
    hashtable_t* ref_table;
};

static void serialize_plist(node_t* node, void* data)
{
    uint64_t *index_val = NULL;
    struct serialize_s *ser = (struct serialize_s *) data;
    uint64_t current_index = ser->objects->len;

    //first check that node is not yet in objects
    void* val = hash_table_lookup(ser->ref_table, node);
    if (val)
    {
        //data is already in table
        return;
    }
    //insert new ref
    index_val = (uint64_t *) malloc(sizeof(uint64_t));
    assert(index_val != NULL);
    *index_val = current_index;
    hash_table_insert(ser->ref_table, node, index_val);

    //now append current node to object array
    ptr_array_add(ser->objects, node);

    //now recurse on children
    node_t *ch;
    for (ch = node_first_child(node); ch; ch = node_next_sibling(ch)) {
        serialize_plist(ch, data);
    }
}

#define Log2(x) (x == 8 ? 3 : (x == 4 ? 2 : (x == 2 ? 1 : 0)))

static void write_int(bytearray_t * bplist, uint64_t val)
{
    int size = get_needed_bytes(val);
    uint8_t sz;
    //do not write 3bytes int node
    if (size == 3)
        size++;
    sz = BPLIST_UINT | Log2(size);

    val = be64toh(val);
    byte_array_append(bplist, &sz, 1);
    byte_array_append(bplist, (uint8_t*)&val + (8-size), size);
}

static void write_uint(bytearray_t * bplist, uint64_t val)
{
    uint8_t sz = BPLIST_UINT | 4;
    uint64_t zero = 0;

    val = be64toh(val);
    byte_array_append(bplist, &sz, 1);
    byte_array_append(bplist, &zero, sizeof(uint64_t));
    byte_array_append(bplist, &val, sizeof(uint64_t));
}

static void write_real(bytearray_t * bplist, double val)
{
    int size = get_real_bytes(val);	//cheat to know used space
    uint8_t buff[16];
    buff[7] = BPLIST_REAL | Log2(size);
    if (size == sizeof(float)) {
        float floatval = (float)val;
        *(uint32_t*)(buff+8) = float_bswap32(*(uint32_t*)&floatval);
    } else {
        *(uint64_t*)(buff+8) = float_bswap64(*(uint64_t*)&val);
    }
    byte_array_append(bplist, buff+7, size+1);
}

static void write_date(bytearray_t * bplist, double val)
{
    uint8_t buff[16];
    buff[7] = BPLIST_DATE | 3;
    *(uint64_t*)(buff+8) = float_bswap64(*(uint64_t*)&val);
    byte_array_append(bplist, buff+7, 9);
}

static void write_raw_data(bytearray_t * bplist, uint8_t mark, uint8_t * val, uint64_t size)
{
    uint8_t marker = mark | (size < 15 ? size : 0xf);
    byte_array_append(bplist, &marker, sizeof(uint8_t));
    if (size >= 15) {
        write_int(bplist, size);
    }
    if (BPLIST_UNICODE==mark) size <<= 1;
    byte_array_append(bplist, val, size);
}

static void write_data(bytearray_t * bplist, uint8_t * val, uint64_t size)
{
    write_raw_data(bplist, BPLIST_DATA, val, size);
}

static void write_string(bytearray_t * bplist, char *val, uint64_t size)
{
    write_raw_data(bplist, BPLIST_STRING, (uint8_t *) val, size);
}

static uint16_t *plist_utf8_to_utf16be(char *unistr, long size, long *items_read, long *items_written)
{
	uint16_t *outbuf;
	int p = 0;
	long i = 0;

	unsigned char c0;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;

	uint32_t w;

	outbuf = (uint16_t*)malloc(((size*2)+1)*sizeof(uint16_t));
	if (!outbuf) {
		PLIST_BIN_ERR("%s: Could not allocate %" PRIu64 " bytes\n", __func__, (uint64_t)((size*2)+1)*sizeof(uint16_t));
		return NULL;
	}

	while (i < size) {
		c0 = unistr[i];
		c1 = (i < size-1) ? unistr[i+1] : 0;
		c2 = (i < size-2) ? unistr[i+2] : 0;
		c3 = (i < size-3) ? unistr[i+3] : 0;
		if ((c0 >= 0xF0) && (i < size-3) && (c1 >= 0x80) && (c2 >= 0x80) && (c3 >= 0x80)) {
			// 4 byte sequence.  Need to generate UTF-16 surrogate pair
			w = ((((c0 & 7) << 18) + ((c1 & 0x3F) << 12) + ((c2 & 0x3F) << 6) + (c3 & 0x3F)) & 0x1FFFFF) - 0x010000;
			outbuf[p++] = be16toh(0xD800 + (w >> 10));
			outbuf[p++] = be16toh(0xDC00 + (w & 0x3FF));
			i+=4;
		} else if ((c0 >= 0xE0) && (i < size-2) && (c1 >= 0x80) && (c2 >= 0x80)) {
			// 3 byte sequence
			outbuf[p++] = be16toh(((c2 & 0x3F) + ((c1 & 3) << 6)) + (((c1 >> 2) & 15) << 8) + ((c0 & 15) << 12));
			i+=3;
		} else if ((c0 >= 0xC0) && (i < size-1) && (c1 >= 0x80)) {
			// 2 byte sequence
			outbuf[p++] = be16toh(((c1 & 0x3F) + ((c0 & 3) << 6)) + (((c0 >> 2) & 7) << 8));
			i+=2;
		} else if (c0 < 0x80) {
			// 1 byte sequence
			outbuf[p++] = be16toh(c0);
			i+=1;
		} else {
			// invalid character
			PLIST_BIN_ERR("%s: invalid utf8 sequence in string at index %lu\n", __func__, i);
			break;
		}
	}
	if (items_read) {
		*items_read = i;
	}
	if (items_written) {
		*items_written = p;
	}
	outbuf[p] = 0;

	return outbuf;
}

static void write_unicode(bytearray_t * bplist, char *val, uint64_t size)
{
    long items_read = 0;
    long items_written = 0;
    uint16_t *unicodestr = NULL;

    unicodestr = plist_utf8_to_utf16be(val, size, &items_read, &items_written);
    write_raw_data(bplist, BPLIST_UNICODE, (uint8_t*)unicodestr, items_written);
    free(unicodestr);
}

static void write_array(bytearray_t * bplist, node_t* node, hashtable_t* ref_table, uint8_t ref_size)
{
    node_t* cur = NULL;
    uint64_t i = 0;

    uint64_t size = node_n_children(node);
    uint8_t marker = BPLIST_ARRAY | (size < 15 ? size : 0xf);
    byte_array_append(bplist, &marker, sizeof(uint8_t));
    if (size >= 15) {
        write_int(bplist, size);
    }

    for (i = 0, cur = node_first_child(node); cur && i < size; cur = node_next_sibling(cur), i++) {
        uint64_t idx = *(uint64_t *) (hash_table_lookup(ref_table, cur));
        idx = be64toh(idx);
        byte_array_append(bplist, (uint8_t*)&idx + (sizeof(uint64_t) - ref_size), ref_size);
    }
}

static void write_dict(bytearray_t * bplist, node_t* node, hashtable_t* ref_table, uint8_t ref_size)
{
    node_t* cur = NULL;
    uint64_t i = 0;

    uint64_t size = node_n_children(node) / 2;
    uint8_t marker = BPLIST_DICT | (size < 15 ? size : 0xf);
    byte_array_append(bplist, &marker, sizeof(uint8_t));
    if (size >= 15) {
        write_int(bplist, size);
    }

    for (i = 0, cur = node_first_child(node); cur && i < size; cur = node_next_sibling(node_next_sibling(cur)), i++) {
        uint64_t idx1 = *(uint64_t *) (hash_table_lookup(ref_table, cur));
        idx1 = be64toh(idx1);
        byte_array_append(bplist, (uint8_t*)&idx1 + (sizeof(uint64_t) - ref_size), ref_size);
    }

    for (i = 0, cur = node_first_child(node); cur && i < size; cur = node_next_sibling(node_next_sibling(cur)), i++) {
        uint64_t idx2 = *(uint64_t *) (hash_table_lookup(ref_table, cur->next));
        idx2 = be64toh(idx2);
        byte_array_append(bplist, (uint8_t*)&idx2 + (sizeof(uint64_t) - ref_size), ref_size);
    }
}

static void write_uid(bytearray_t * bplist, uint64_t val)
{
    val = (uint32_t)val;
    int size = get_needed_bytes(val);
    uint8_t sz;
    //do not write 3bytes int node
    if (size == 3)
        size++;
    sz = BPLIST_UID | (size-1); // yes, this is what Apple does...

    val = be64toh(val);
    byte_array_append(bplist, &sz, 1);
    byte_array_append(bplist, (uint8_t*)&val + (8-size), size);
}

static int is_ascii_string(char* s, int len)
{
  int ret = 1, i = 0;
  for(i = 0; i < len; i++)
  {
      if ( !isascii( s[i] ) )
      {
          ret = 0;
          break;
      }
  }
  return ret;
}

PLIST_API void plist_to_bin(plist_t plist, char **plist_bin, uint32_t * length)
{
    ptrarray_t* objects = NULL;
    hashtable_t* ref_table = NULL;
    struct serialize_s ser_s;
    uint8_t offset_size = 0;
    uint8_t ref_size = 0;
    uint64_t num_objects = 0;
    uint64_t root_object = 0;
    uint64_t offset_table_index = 0;
    bytearray_t *bplist_buff = NULL;
    uint64_t i = 0;
    uint64_t buff_len = 0;
    uint64_t *offsets = NULL;
    bplist_trailer_t trailer;
    uint64_t objects_len = 0;

    //check for valid input
    if (!plist || !plist_bin || *plist_bin || !length)
        return;

    //list of objects
    objects = ptr_array_new(4096);
    //hashtable to write only once same nodes
    ref_table = hash_table_new(plist_data_hash, plist_data_compare, free);

    //serialize plist
    ser_s.objects = objects;
    ser_s.ref_table = ref_table;
    serialize_plist((node_t*)plist, &ser_s);

    //now stream to output buffer
    offset_size = 0;			//unknown yet
    objects_len = objects->len;
    ref_size = get_needed_bytes(objects_len);
    num_objects = objects->len;
    root_object = 0;			//root is first in list
    offset_table_index = 0;		//unknown yet

    //figure out the storage size required
    uint64_t req = 0;
    for (i = 0; i < num_objects; i++)
    {
        node_t* node = (node_t*)ptr_array_index(objects, i);
        plist_data_t data = plist_get_data(node);
        uint64_t size;
        uint8_t bsize;
        switch (data->type)
        {
        case PLIST_BOOLEAN:
            req += 1;
            break;
        case PLIST_KEY:
        case PLIST_STRING:
            req += 1;
            if (data->length >= 15) {
                bsize = get_needed_bytes(data->length);
                if (bsize == 3) bsize = 4;
                req += 1;
                req += bsize;
            }
            if ( is_ascii_string(data->strval, data->length) )
            {
                req += data->length;
            }
            else
            {
                req += data->length * 2;
            }
            break;
        case PLIST_REAL:
            size = get_real_bytes(data->realval);
            req += 1;
            req += size;
            break;
        case PLIST_DATE:
            req += 9;
            break;
        case PLIST_ARRAY:
            size = node_n_children(node);
            req += 1;
            if (size >= 15) {
                bsize = get_needed_bytes(size);
                if (bsize == 3) bsize = 4;
                req += 1;
                req += bsize;
            }
            req += size * ref_size;
            break;
        case PLIST_DICT:
            size = node_n_children(node) / 2;
            req += 1;
            if (size >= 15) {
                bsize = get_needed_bytes(size);
                if (bsize == 3) bsize = 4;
                req += 1;
                req += bsize;
            }
            req += size * 2 * ref_size;
            break;
        default:
            size = data->length;
            req += 1;
            if (size >= 15) {
                bsize = get_needed_bytes(size);
                if (bsize == 3) bsize = 4;
                req += 1;
                req += bsize;
            }
            req += data->length;
            break;
        }
    }
    // add size of magic
    req += BPLIST_MAGIC_SIZE;
    req += BPLIST_VERSION_SIZE;
    // add size of offset table
    req += get_needed_bytes(req) * num_objects;
    // add size of trailer
    req += sizeof(bplist_trailer_t);

    //setup a dynamic bytes array to store bplist in
    bplist_buff = byte_array_new(req);

    //set magic number and version
    byte_array_append(bplist_buff, BPLIST_MAGIC, BPLIST_MAGIC_SIZE);
    byte_array_append(bplist_buff, BPLIST_VERSION, BPLIST_VERSION_SIZE);

    //write objects and table
    offsets = (uint64_t *) malloc(num_objects * sizeof(uint64_t));
    assert(offsets != NULL);
    for (i = 0; i < num_objects; i++)
    {

        plist_data_t data = plist_get_data(ptr_array_index(objects, i));
        offsets[i] = bplist_buff->len;

        switch (data->type)
        {
        case PLIST_BOOLEAN: {
            uint8_t b = data->boolval ? BPLIST_TRUE : BPLIST_FALSE;
            byte_array_append(bplist_buff, &b, 1);
            break;
        }
        case PLIST_UINT:
            if (data->length == 16) {
                write_uint(bplist_buff, data->intval);
            } else {
                write_int(bplist_buff, data->intval);
            }
            break;

        case PLIST_REAL:
            write_real(bplist_buff, data->realval);
            break;

        case PLIST_KEY:
        case PLIST_STRING:
            if ( is_ascii_string(data->strval, data->length) )
            {
                write_string(bplist_buff, data->strval, data->length);
            }
            else
            {
                write_unicode(bplist_buff, data->strval, data->length);
            }
            break;
        case PLIST_DATA:
            write_data(bplist_buff, data->buff, data->length);
            break;
        case PLIST_ARRAY:
            write_array(bplist_buff, (node_t*)ptr_array_index(objects, i), ref_table, ref_size);
            break;
        case PLIST_DICT:
            write_dict(bplist_buff, (node_t*)ptr_array_index(objects, i), ref_table, ref_size);
            break;
        case PLIST_DATE:
            write_date(bplist_buff, data->realval);
            break;
        case PLIST_UID:
            write_uid(bplist_buff, data->intval);
            break;
        default:
            break;
        }
    }

    //free intermediate objects
    ptr_array_free(objects);
    hash_table_destroy(ref_table);

    //write offsets
    buff_len = bplist_buff->len;
    offset_size = get_needed_bytes(buff_len);
    offset_table_index = bplist_buff->len;
    for (i = 0; i < num_objects; i++) {
        uint64_t offset = be64toh(offsets[i]);
        byte_array_append(bplist_buff, (uint8_t*)&offset + (sizeof(uint64_t) - offset_size), offset_size);
    }
    free(offsets);

    //setup trailer
    memset(trailer.unused, '\0', sizeof(trailer.unused));
    trailer.offset_size = offset_size;
    trailer.ref_size = ref_size;
    trailer.num_objects = be64toh(num_objects);
    trailer.root_object_index = be64toh(root_object);
    trailer.offset_table_offset = be64toh(offset_table_index);

    byte_array_append(bplist_buff, &trailer, sizeof(bplist_trailer_t));

    //set output buffer and size
    *plist_bin = (char*)bplist_buff->data;
    *length = bplist_buff->len;

    bplist_buff->data = NULL; // make sure we don't free the output buffer
    byte_array_free(bplist_buff);
}

PLIST_API void plist_to_bin_free(char *plist_bin)
{
    free(plist_bin);
}
