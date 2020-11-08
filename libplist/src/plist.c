/*
 * plist.c
 * Builds plist XML structures
 *
 * Copyright (c) 2009-2019 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2010-2015 Martin Szulecki, All Rights Reserved.
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

#define _GNU_SOURCE 1
#include <string.h>
#include "plist.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <float.h>

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <node.h>
#include <hashtable.h>
#include <ptrarray.h>

extern void plist_xml_init(void);
extern void plist_xml_deinit(void);
extern void plist_bin_init(void);
extern void plist_bin_deinit(void);

static void internal_plist_init(void)
{
    plist_bin_init();
    plist_xml_init();
}

static void internal_plist_deinit(void)
{
    plist_bin_deinit();
    plist_xml_deinit();
}

#ifdef WIN32

typedef volatile struct {
    LONG lock;
    int state;
} thread_once_t;

static thread_once_t init_once = {0, 0};
static thread_once_t deinit_once = {0, 0};

void thread_once(thread_once_t *once_control, void (*init_routine)(void))
{
    while (InterlockedExchange(&(once_control->lock), 1) != 0) {
        Sleep(1);
    }
    if (!once_control->state) {
        once_control->state = 1;
        init_routine();
    }
    InterlockedExchange(&(once_control->lock), 0);
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        thread_once(&init_once, internal_plist_init);
        break;
    case DLL_PROCESS_DETACH:
        thread_once(&deinit_once, internal_plist_deinit);
        break;
    default:
        break;
    }
    return 1;
}

#else

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_once_t deinit_once = PTHREAD_ONCE_INIT;

static void __attribute__((constructor)) libplist_initialize(void)
{
    pthread_once(&init_once, internal_plist_init);
}

static void __attribute__((destructor)) libplist_deinitialize(void)
{
    pthread_once(&deinit_once, internal_plist_deinit);
}

#endif

#ifndef HAVE_MEMMEM
// see https://sourceware.org/legacy-ml/libc-alpha/2007-12/msg00000.html

#ifndef _LIBC
# define __builtin_expect(expr, val)   (expr)
#endif

#undef memmem

/* Return the first occurrence of NEEDLE in HAYSTACK. */
void* memmem(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len)
{
    /* not really Rabin-Karp, just using additive hashing */
    char* haystack_ = (char*)haystack;
    char* needle_ = (char*)needle;
    int hash = 0;  /* this is the static hash value of the needle */
    int hay_hash = 0;  /* rolling hash over the haystack */
    char* last;
    size_t i;

    if (haystack_len < needle_len)
        return NULL;

    if (!needle_len)
        return haystack_;

    /* initialize hashes */
    for (i = needle_len; i; --i) {
        hash += *needle_++;
        hay_hash += *haystack_++;
    }

    /* iterate over the haystack */
    haystack_ = (char*)haystack;
    needle_ = (char*)needle;
    last = haystack_+(haystack_len - needle_len + 1);
    for (; haystack_ < last; ++haystack_) {
        if (__builtin_expect(hash == hay_hash, 0)
              && *haystack_ == *needle_ /* prevent calling memcmp, was a optimization from existing glibc */
              && !memcmp (haystack_, needle_, needle_len)) {
            return haystack_;
        }
        /* roll the hash */
        hay_hash -= *haystack_;
        hay_hash += *(haystack_+needle_len);
    }
    return NULL;
}
#endif

PLIST_API int plist_is_binary(const char *plist_data, uint32_t length)
{
    if (length < 8) {
        return 0;
    }

    return (memcmp(plist_data, "bplist00", 8) == 0);
}


PLIST_API void plist_from_memory(const char *plist_data, uint32_t length, plist_t * plist)
{
    if (length < 8) {
        *plist = NULL;
        return;
    }

    if (plist_is_binary(plist_data, length)) {
        plist_from_bin(plist_data, length, plist);
    } else {
        plist_from_xml(plist_data, length, plist);
    }
}

plist_t plist_new_node(plist_data_t data)
{
    return (plist_t) node_create(NULL, data);
}

plist_data_t plist_get_data(const plist_t node)
{
    if (!node)
        return NULL;
    return (plist_data_t)((node_t*)node)->data;
}

plist_data_t plist_new_plist_data(void)
{
    plist_data_t data = (plist_data_t) calloc(sizeof(struct plist_data_s), 1);
    return data;
}

static unsigned int dict_key_hash(const void *data)
{
    plist_data_t keydata = (plist_data_t)data;
    unsigned int hash = 5381;
    size_t i;
    char *str = keydata->strval;
    for (i = 0; i < keydata->length; str++, i++) {
        hash = ((hash << 5) + hash) + *str;
    }
    return hash;
}

static int dict_key_compare(const void* a, const void* b)
{
    plist_data_t data_a = (plist_data_t)a;
    plist_data_t data_b = (plist_data_t)b;
    if (data_a->strval == NULL || data_b->strval == NULL) {
        return FALSE;
    }
    if (data_a->length != data_b->length) {
        return FALSE;
    }
    return (strcmp(data_a->strval, data_b->strval) == 0) ? TRUE : FALSE;
}

void plist_free_data(plist_data_t data)
{
    if (data)
    {
        switch (data->type)
        {
        case PLIST_KEY:
        case PLIST_STRING:
            free(data->strval);
            break;
        case PLIST_DATA:
            free(data->buff);
            break;
        case PLIST_ARRAY:
            ptr_array_free((ptrarray_t*)data->hashtable);
            break;
        case PLIST_DICT:
            hash_table_destroy((hashtable_t*)data->hashtable);
            break;
        default:
            break;
        }
        free(data);
    }
}

static int plist_free_node(node_t* node)
{
    plist_data_t data = NULL;
    int node_index = node_detach(node->parent, node);
    data = plist_get_data(node);
    plist_free_data(data);
    node->data = NULL;

    node_t *ch;
    for (ch = node_first_child(node); ch; ) {
        node_t *next = node_next_sibling(ch);
        plist_free_node(ch);
        ch = next;
    }

    node_destroy(node);

    return node_index;
}

PLIST_API plist_t plist_new_dict(void)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_DICT;
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_array(void)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_ARRAY;
    return plist_new_node(data);
}

//These nodes should not be handled by users
static plist_t plist_new_key(const char *val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_KEY;
    data->strval = strdup(val);
    data->length = strlen(val);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_string(const char *val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_STRING;
    data->strval = strdup(val);
    data->length = strlen(val);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_bool(uint8_t val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_BOOLEAN;
    data->boolval = val;
    data->length = sizeof(uint8_t);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_uint(uint64_t val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_UINT;
    data->intval = val;
    data->length = sizeof(uint64_t);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_uid(uint64_t val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_UID;
    data->intval = val;
    data->length = sizeof(uint64_t);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_real(double val)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_REAL;
    data->realval = val;
    data->length = sizeof(double);
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_data(const char *val, uint64_t length)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_DATA;
    data->buff = (uint8_t *) malloc(length);
    memcpy(data->buff, val, length);
    data->length = length;
    return plist_new_node(data);
}

PLIST_API plist_t plist_new_date(int32_t sec, int32_t usec)
{
    plist_data_t data = plist_new_plist_data();
    data->type = PLIST_DATE;
    data->realval = (double)sec + (double)usec / 1000000;
    data->length = sizeof(double);
    return plist_new_node(data);
}

PLIST_API void plist_free(plist_t plist)
{
    if (plist)
    {
        plist_free_node((node_t*)plist);
    }
}

static plist_t plist_copy_node(node_t *node)
{
    plist_type node_type = PLIST_NONE;
    plist_t newnode = NULL;
    plist_data_t data = plist_get_data(node);
    plist_data_t newdata = plist_new_plist_data();

    assert(data);				// plist should always have data
    assert(newdata);

    memcpy(newdata, data, sizeof(struct plist_data_s));

    node_type = plist_get_node_type(node);
    switch (node_type) {
        case PLIST_DATA:
            newdata->buff = (uint8_t *) malloc(data->length);
            memcpy(newdata->buff, data->buff, data->length);
            break;
        case PLIST_KEY:
        case PLIST_STRING:
            newdata->strval = strdup((char *) data->strval);
            break;
        case PLIST_ARRAY:
            if (data->hashtable) {
                ptrarray_t* pa = ptr_array_new(((ptrarray_t*)data->hashtable)->capacity);
                assert(pa);
                newdata->hashtable = pa;
            }
            break;
        case PLIST_DICT:
            if (data->hashtable) {
                hashtable_t* ht = hash_table_new(dict_key_hash, dict_key_compare, NULL);
                assert(ht);
                newdata->hashtable = ht;
            }
            break;
        default:
            break;
    }
    newnode = plist_new_node(newdata);

    node_t *ch;
    unsigned int node_index = 0;
    for (ch = node_first_child(node); ch; ch = node_next_sibling(ch)) {
        /* copy child node */
        plist_t newch = plist_copy_node(ch);
        /* attach to new parent node */
        node_attach((node_t*)newnode, (node_t*)newch);
        /* if needed, add child node to lookup table of parent node */
        switch (node_type) {
            case PLIST_ARRAY:
                if (newdata->hashtable) {
                    ptr_array_add((ptrarray_t*)newdata->hashtable, newch);
                }
                break;
            case PLIST_DICT:
                if (newdata->hashtable && (node_index % 2 != 0)) {
                    hash_table_insert((hashtable_t*)newdata->hashtable, (node_prev_sibling((node_t*)newch))->data, newch);
                }
                break;
            default:
                break;
        }
        node_index++;
    }
    return newnode;
}

PLIST_API plist_t plist_copy(plist_t node)
{
    return node ? plist_copy_node((node_t*)node) : NULL;
}

PLIST_API uint32_t plist_array_get_size(plist_t node)
{
    uint32_t ret = 0;
    if (node && PLIST_ARRAY == plist_get_node_type(node))
    {
        ret = node_n_children((node_t*)node);
    }
    return ret;
}

PLIST_API plist_t plist_array_get_item(plist_t node, uint32_t n)
{
    plist_t ret = NULL;
    if (node && PLIST_ARRAY == plist_get_node_type(node) && n < INT_MAX)
    {
        ptrarray_t *pa = (ptrarray_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
        if (pa) {
            ret = (plist_t)ptr_array_index(pa, n);
        } else {
            ret = (plist_t)node_nth_child((node_t*)node, n);
        }
    }
    return ret;
}

PLIST_API uint32_t plist_array_get_item_index(plist_t node)
{
    plist_t father = plist_get_parent(node);
    if (PLIST_ARRAY == plist_get_node_type(father))
    {
        return node_child_position((node_t*)father, (node_t*)node);
    }
    return UINT_MAX;
}

static void _plist_array_post_insert(plist_t node, plist_t item, long n)
{
    ptrarray_t *pa = (ptrarray_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
    if (pa) {
        /* store pointer to item in array */
        ptr_array_insert(pa, item, n);
    } else {
        if (((node_t*)node)->count > 100) {
            /* make new lookup array */
            pa = ptr_array_new(128);
            plist_t current = NULL;
            for (current = (plist_t)node_first_child((node_t*)node);
                 pa && current;
                 current = (plist_t)node_next_sibling((node_t*)current))
            {
                ptr_array_add(pa, current);
            }
            ((plist_data_t)((node_t*)node)->data)->hashtable = pa;
        }
    }
}

PLIST_API void plist_array_set_item(plist_t node, plist_t item, uint32_t n)
{
    if (node && PLIST_ARRAY == plist_get_node_type(node) && n < INT_MAX)
    {
        plist_t old_item = plist_array_get_item(node, n);
        if (old_item)
        {
            int idx = plist_free_node((node_t*)old_item);
            assert(idx >= 0);
            if (idx < 0) {
                return;
            } else {
                node_insert((node_t*)node, idx, (node_t*)item);
                ptrarray_t* pa = (ptrarray_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
                if (pa) {
                    ptr_array_set(pa, (node_t*)item, idx);
                }
            }
        }
    }
}

PLIST_API void plist_array_append_item(plist_t node, plist_t item)
{
    if (node && PLIST_ARRAY == plist_get_node_type(node))
    {
        node_attach((node_t*)node, (node_t*)item);
        _plist_array_post_insert(node, item, -1);
    }
}

PLIST_API void plist_array_insert_item(plist_t node, plist_t item, uint32_t n)
{
    if (node && PLIST_ARRAY == plist_get_node_type(node) && n < INT_MAX)
    {
        node_insert((node_t*)node, n, (node_t*)item);
        _plist_array_post_insert(node, item, (long)n);
    }
}

PLIST_API void plist_array_remove_item(plist_t node, uint32_t n)
{
    if (node && PLIST_ARRAY == plist_get_node_type(node) && n < INT_MAX)
    {
        plist_t old_item = plist_array_get_item(node, n);
        if (old_item)
        {
            ptrarray_t* pa = (ptrarray_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
            if (pa) {
                ptr_array_remove(pa, n);
            }
            plist_free(old_item);
        }
    }
}

PLIST_API void plist_array_item_remove(plist_t node)
{
    plist_t father = plist_get_parent(node);
    if (PLIST_ARRAY == plist_get_node_type(father))
    {
        int n = node_child_position((node_t*)father, (node_t*)node);
        if (n < 0) return;
        ptrarray_t* pa = (ptrarray_t*)((plist_data_t)((node_t*)father)->data)->hashtable;
        if (pa) {
            ptr_array_remove(pa, n);
        }
        plist_free(node);
    }
}

PLIST_API void plist_array_new_iter(plist_t node, plist_array_iter *iter)
{
    if (iter)
    {
        *iter = malloc(sizeof(node_t*));
        *((node_t**)(*iter)) = node_first_child((node_t*)node);
    }
}

PLIST_API void plist_array_next_item(plist_t node, plist_array_iter iter, plist_t *item)
{
    node_t** iter_node = (node_t**)iter;

    if (item)
    {
        *item = NULL;
    }

    if (node && PLIST_ARRAY == plist_get_node_type(node) && *iter_node)
    {
        if (item)
        {
            *item = (plist_t)(*iter_node);
        }
        *iter_node = node_next_sibling(*iter_node);
    }
}

PLIST_API uint32_t plist_dict_get_size(plist_t node)
{
    uint32_t ret = 0;
    if (node && PLIST_DICT == plist_get_node_type(node))
    {
        ret = node_n_children((node_t*)node) / 2;
    }
    return ret;
}

PLIST_API void plist_dict_new_iter(plist_t node, plist_dict_iter *iter)
{
    if (iter)
    {
        *iter = malloc(sizeof(node_t*));
        *((node_t**)(*iter)) = node_first_child((node_t*)node);
    }
}

PLIST_API void plist_dict_next_item(plist_t node, plist_dict_iter iter, char **key, plist_t *val)
{
    node_t** iter_node = (node_t**)iter;

    if (key)
    {
        *key = NULL;
    }
    if (val)
    {
        *val = NULL;
    }

    if (node && PLIST_DICT == plist_get_node_type(node) && *iter_node)
    {
        if (key)
        {
            plist_get_key_val((plist_t)(*iter_node), key);
        }
        *iter_node = node_next_sibling(*iter_node);
        if (val)
        {
            *val = (plist_t)(*iter_node);
        }
        *iter_node = node_next_sibling(*iter_node);
    }
}

PLIST_API void plist_dict_get_item_key(plist_t node, char **key)
{
    plist_t father = plist_get_parent(node);
    if (PLIST_DICT == plist_get_node_type(father))
    {
        plist_get_key_val( (plist_t) node_prev_sibling((node_t*)node), key);
    }
}

PLIST_API plist_t plist_dict_item_get_key(plist_t node)
{
    plist_t ret = NULL;
    plist_t father = plist_get_parent(node);
    if (PLIST_DICT == plist_get_node_type(father))
    {
        ret = (plist_t)node_prev_sibling((node_t*)node);
    }
    return ret;
}

PLIST_API plist_t plist_dict_get_item(plist_t node, const char* key)
{
    plist_t ret = NULL;

    if (node && PLIST_DICT == plist_get_node_type(node))
    {
        plist_data_t data = plist_get_data(node);
        hashtable_t *ht = (hashtable_t*)data->hashtable;
        if (ht) {
            struct plist_data_s sdata;
            sdata.strval = (char*)key;
            sdata.length = strlen(key);
            ret = (plist_t)hash_table_lookup(ht, &sdata);
        } else {
            plist_t current = NULL;
        for (current = (plist_t)node_first_child((node_t*)node);
                current;
                current = (plist_t)node_next_sibling(node_next_sibling((node_t*)current)))
            {
                data = plist_get_data(current);
                assert( PLIST_KEY == plist_get_node_type(current) );

                if (data && !strcmp(key, data->strval))
                {
                    ret = (plist_t)node_next_sibling((node_t*)current);
                    break;
                }
            }
        }
    }
    return ret;
}

PLIST_API void plist_dict_set_item(plist_t node, const char* key, plist_t item)
{
    if (node && PLIST_DICT == plist_get_node_type(node)) {
        node_t* old_item = (node_t*)plist_dict_get_item(node, key);
        plist_t key_node = NULL;
        if (old_item) {
            int idx = plist_free_node(old_item);
            assert(idx >= 0);
            if (idx < 0) {
                return;
            } else {
                node_insert((node_t*)node, idx, (node_t*)item);
            }
            key_node = node_prev_sibling((node_t*)item);
        } else {
            key_node = plist_new_key(key);
            node_attach((node_t*)node, (node_t*)key_node);
            node_attach((node_t*)node, (node_t*)item);
        }

        hashtable_t *ht = (hashtable_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
        if (ht) {
            /* store pointer to item in hash table */
            hash_table_insert(ht, (plist_data_t)((node_t*)key_node)->data, item);
        } else {
            if (((node_t*)node)->count > 500) {
                /* make new hash table */
                ht = hash_table_new(dict_key_hash, dict_key_compare, NULL);
                /* calculate the hashes for all entries we have so far */
                plist_t current = NULL;
                for (current = (plist_t)node_first_child((node_t*)node);
                     ht && current;
                     current = (plist_t)node_next_sibling(node_next_sibling((node_t*)current)))
                {
                    hash_table_insert(ht, ((node_t*)current)->data, node_next_sibling((node_t*)current));
                }
                ((plist_data_t)((node_t*)node)->data)->hashtable = ht;
            }
        }
    }
}

PLIST_API void plist_dict_insert_item(plist_t node, const char* key, plist_t item)
{
    plist_dict_set_item(node, key, item);
}

PLIST_API void plist_dict_remove_item(plist_t node, const char* key)
{
    if (node && PLIST_DICT == plist_get_node_type(node))
    {
        plist_t old_item = plist_dict_get_item(node, key);
        if (old_item)
        {
            plist_t key_node = node_prev_sibling((node_t*)old_item);
            hashtable_t* ht = (hashtable_t*)((plist_data_t)((node_t*)node)->data)->hashtable;
            if (ht) {
                hash_table_remove(ht, ((node_t*)key_node)->data);
            }
            plist_free(key_node);
            plist_free(old_item);
        }
    }
}

PLIST_API void plist_dict_merge(plist_t *target, plist_t source)
{
	if (!target || !*target || (plist_get_node_type(*target) != PLIST_DICT) || !source || (plist_get_node_type(source) != PLIST_DICT))
		return;

	char* key = NULL;
	plist_dict_iter it = NULL;
	plist_t subnode = NULL;
	plist_dict_new_iter(source, &it);
	if (!it)
		return;

	do {
		plist_dict_next_item(source, it, &key, &subnode);
		if (!key)
			break;

		plist_dict_set_item(*target, key, plist_copy(subnode));
		free(key);
		key = NULL;
	} while (1);
	free(it);
}

PLIST_API plist_t plist_access_pathv(plist_t plist, uint32_t length, va_list v)
{
    plist_t current = plist;
    plist_type type = PLIST_NONE;
    uint32_t i = 0;

    for (i = 0; i < length && current; i++)
    {
        type = plist_get_node_type(current);

        if (type == PLIST_ARRAY)
        {
            uint32_t n = va_arg(v, uint32_t);
            current = plist_array_get_item(current, n);
        }
        else if (type == PLIST_DICT)
        {
            const char* key = va_arg(v, const char*);
            current = plist_dict_get_item(current, key);
        }
    }
    return current;
}

PLIST_API plist_t plist_access_path(plist_t plist, uint32_t length, ...)
{
    plist_t ret = NULL;
    va_list v;

    va_start(v, length);
    ret = plist_access_pathv(plist, length, v);
    va_end(v);
    return ret;
}

static void plist_get_type_and_value(plist_t node, plist_type * type, void *value, uint64_t * length)
{
    plist_data_t data = NULL;

    if (!node)
        return;

    data = plist_get_data(node);

    *type = data->type;
    *length = data->length;

    switch (*type)
    {
    case PLIST_BOOLEAN:
        *((char *) value) = data->boolval;
        break;
    case PLIST_UINT:
    case PLIST_UID:
        *((uint64_t *) value) = data->intval;
        break;
    case PLIST_REAL:
    case PLIST_DATE:
        *((double *) value) = data->realval;
        break;
    case PLIST_KEY:
    case PLIST_STRING:
        *((char **) value) = strdup(data->strval);
        break;
    case PLIST_DATA:
        *((uint8_t **) value) = (uint8_t *) malloc(*length * sizeof(uint8_t));
        memcpy(*((uint8_t **) value), data->buff, *length * sizeof(uint8_t));
        break;
    case PLIST_ARRAY:
    case PLIST_DICT:
    default:
        break;
    }
}

PLIST_API plist_t plist_get_parent(plist_t node)
{
    return node ? (plist_t) ((node_t*) node)->parent : NULL;
}

PLIST_API plist_type plist_get_node_type(plist_t node)
{
    if (node)
    {
        plist_data_t data = plist_get_data(node);
        if (data)
            return data->type;
    }
    return PLIST_NONE;
}

PLIST_API void plist_get_key_val(plist_t node, char **val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_KEY != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    if (!*val)
        return;
    assert(length == strlen(*val));
}

PLIST_API void plist_get_string_val(plist_t node, char **val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_STRING != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    if (!*val)
        return;
    assert(length == strlen(*val));
}

PLIST_API const char* plist_get_string_ptr(plist_t node, uint64_t* length)
{
    if (!node)
        return NULL;
    plist_type type = plist_get_node_type(node);
    if (PLIST_STRING != type)
        return NULL;
    plist_data_t data = plist_get_data(node);
    if (length)
        *length = data->length;
    return (const char*)data->strval;
}

PLIST_API void plist_get_bool_val(plist_t node, uint8_t * val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_BOOLEAN != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    assert(length == sizeof(uint8_t));
}

PLIST_API void plist_get_uint_val(plist_t node, uint64_t * val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_UINT != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    assert(length == sizeof(uint64_t) || length == 16);
}

PLIST_API void plist_get_uid_val(plist_t node, uint64_t * val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_UID != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    assert(length == sizeof(uint64_t));
}

PLIST_API void plist_get_real_val(plist_t node, double *val)
{
    if (!node || !val)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    if (PLIST_REAL != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, &length);
    assert(length == sizeof(double));
}

PLIST_API void plist_get_data_val(plist_t node, char **val, uint64_t * length)
{
    if (!node || !val || !length)
        return;
    plist_type type = plist_get_node_type(node);
    if (PLIST_DATA != type)
        return;
    plist_get_type_and_value(node, &type, (void *) val, length);
}

PLIST_API const char* plist_get_data_ptr(plist_t node, uint64_t* length)
{
    if (!node || !length)
        return NULL;
    plist_type type = plist_get_node_type(node);
    if (PLIST_DATA != type)
        return NULL;
    plist_data_t data = plist_get_data(node);
    *length = data->length;
    return (const char*)data->buff;
}

PLIST_API void plist_get_date_val(plist_t node, int32_t * sec, int32_t * usec)
{
    if (!node)
        return;
    plist_type type = plist_get_node_type(node);
    uint64_t length = 0;
    double val = 0;
    if (PLIST_DATE != type)
        return;
    plist_get_type_and_value(node, &type, (void *) &val, &length);
    assert(length == sizeof(double));
    if (sec)
        *sec = (int32_t)val;
    if (usec)
        *usec = (int32_t)fabs((val - (int64_t)val) * 1000000);
}

int plist_data_compare(const void *a, const void *b)
{
    plist_data_t val_a = NULL;
    plist_data_t val_b = NULL;

    if (!a || !b)
        return FALSE;

    if (!((node_t*) a)->data || !((node_t*) b)->data)
        return FALSE;

    val_a = plist_get_data((plist_t) a);
    val_b = plist_get_data((plist_t) b);

    if (val_a->type != val_b->type)
        return FALSE;

    switch (val_a->type)
    {
    case PLIST_BOOLEAN:
    case PLIST_UINT:
    case PLIST_REAL:
    case PLIST_DATE:
    case PLIST_UID:
        if (val_a->length != val_b->length)
            return FALSE;
        if (val_a->intval == val_b->intval)	//it is an union so this is sufficient
            return TRUE;
        else
            return FALSE;

    case PLIST_KEY:
    case PLIST_STRING:
        if (!strcmp(val_a->strval, val_b->strval))
            return TRUE;
        else
            return FALSE;

    case PLIST_DATA:
        if (val_a->length != val_b->length)
            return FALSE;
        if (!memcmp(val_a->buff, val_b->buff, val_a->length))
            return TRUE;
        else
            return FALSE;
    case PLIST_ARRAY:
    case PLIST_DICT:
        //compare pointer
        if (a == b)
            return TRUE;
        else
            return FALSE;
        break;
    default:
        break;
    }
    return FALSE;
}

PLIST_API char plist_compare_node_value(plist_t node_l, plist_t node_r)
{
    return plist_data_compare(node_l, node_r);
}

static void plist_set_element_val(plist_t node, plist_type type, const void *value, uint64_t length)
{
    //free previous allocated buffer
    plist_data_t data = plist_get_data(node);
    assert(data);				// a node should always have data attached

    switch (data->type)
    {
    case PLIST_KEY:
    case PLIST_STRING:
        free(data->strval);
        data->strval = NULL;
        break;
    case PLIST_DATA:
        free(data->buff);
        data->buff = NULL;
        break;
    default:
        break;
    }

    //now handle value

    data->type = type;
    data->length = length;

    switch (type)
    {
    case PLIST_BOOLEAN:
        data->boolval = *((char *) value);
        break;
    case PLIST_UINT:
    case PLIST_UID:
        data->intval = *((uint64_t *) value);
        break;
    case PLIST_REAL:
    case PLIST_DATE:
        data->realval = *((double *) value);
        break;
    case PLIST_KEY:
    case PLIST_STRING:
        data->strval = strdup((char *) value);
        break;
    case PLIST_DATA:
        data->buff = (uint8_t *) malloc(length);
        memcpy(data->buff, value, length);
        break;
    case PLIST_ARRAY:
    case PLIST_DICT:
    default:
        break;
    }
}

PLIST_API void plist_set_key_val(plist_t node, const char *val)
{
    plist_t father = plist_get_parent(node);
    plist_t item = plist_dict_get_item(father, val);
    if (item) {
        return;
    }
    plist_set_element_val(node, PLIST_KEY, val, strlen(val));
}

PLIST_API void plist_set_string_val(plist_t node, const char *val)
{
    plist_set_element_val(node, PLIST_STRING, val, strlen(val));
}

PLIST_API void plist_set_bool_val(plist_t node, uint8_t val)
{
    plist_set_element_val(node, PLIST_BOOLEAN, &val, sizeof(uint8_t));
}

PLIST_API void plist_set_uint_val(plist_t node, uint64_t val)
{
    plist_set_element_val(node, PLIST_UINT, &val, sizeof(uint64_t));
}

PLIST_API void plist_set_uid_val(plist_t node, uint64_t val)
{
    plist_set_element_val(node, PLIST_UID, &val, sizeof(uint64_t));
}

PLIST_API void plist_set_real_val(plist_t node, double val)
{
    plist_set_element_val(node, PLIST_REAL, &val, sizeof(double));
}

PLIST_API void plist_set_data_val(plist_t node, const char *val, uint64_t length)
{
    plist_set_element_val(node, PLIST_DATA, val, length);
}

PLIST_API void plist_set_date_val(plist_t node, int32_t sec, int32_t usec)
{
    double val = (double)sec + (double)usec / 1000000;
    plist_set_element_val(node, PLIST_DATE, &val, sizeof(struct timeval));
}

PLIST_API int plist_bool_val_is_true(plist_t boolnode)
{
    if (!PLIST_IS_BOOLEAN(boolnode)) {
        return 0;
    }
    uint8_t bv = 0;
    plist_get_bool_val(boolnode, &bv);
    return (bv == 1);
}

PLIST_API int plist_uint_val_compare(plist_t uintnode, uint64_t cmpval)
{
    if (!PLIST_IS_UINT(uintnode)) {
        return -1;
    }
    uint64_t uintval = 0;
    plist_get_uint_val(uintnode, &uintval);
    if (uintval == cmpval) {
        return 0;
    } else if (uintval < cmpval) {
        return -1;
    } else {
        return 1;
    }
}

PLIST_API int plist_uid_val_compare(plist_t uidnode, uint64_t cmpval)
{
    if (!PLIST_IS_UID(uidnode)) {
        return -1;
    }
    uint64_t uidval = 0;
    plist_get_uid_val(uidnode, &uidval);
    if (uidval == cmpval) {
        return 0;
    } else if (uidval < cmpval) {
        return -1;
    } else {
        return 1;
    }
}

PLIST_API int plist_real_val_compare(plist_t realnode, double cmpval)
{
    if (!PLIST_IS_REAL(realnode)) {
        return -1;
    }
    double a = 0;
    double b = cmpval;
    plist_get_real_val(realnode, &a);
    double abs_a = fabs(a);
    double abs_b = fabs(b);
    double diff = fabs(a - b);
    if (a == b) {
        return 0;
    } else if (a == 0 || b == 0 || (abs_a + abs_b < DBL_MIN)) {
        if (diff < (DBL_EPSILON * DBL_MIN)) {
            return 0;
        } else if (a < b) {
            return -1;
        }
    } else {
        if ((diff / fmin(abs_a + abs_b, DBL_MAX)) < DBL_EPSILON) {
            return 0;
        } else if (a < b) {
            return -1;
        }
    }
    return 1;
}

PLIST_API int plist_date_val_compare(plist_t datenode, int32_t cmpsec, int32_t cmpusec)
{
    if (!PLIST_IS_DATE(datenode)) {
        return -1;
    }
    int32_t sec = 0;
    int32_t usec = 0;
    plist_get_date_val(datenode, &sec, &usec);
    uint64_t dateval = ((int64_t)sec << 32) | usec;
    uint64_t cmpval = ((int64_t)cmpsec << 32) | cmpusec;
    if (dateval == cmpval) {
        return 0;
    } else if (dateval < cmpval) {
        return -1;
    } else {
        return 1;
    }
}

PLIST_API int plist_string_val_compare(plist_t strnode, const char* cmpval)
{
    if (!PLIST_IS_STRING(strnode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(strnode);
    return strcmp(data->strval, cmpval);
}

PLIST_API int plist_string_val_compare_with_size(plist_t strnode, const char* cmpval, size_t n)
{
    if (!PLIST_IS_STRING(strnode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(strnode);
    return strncmp(data->strval, cmpval, n);
}

PLIST_API int plist_string_val_contains(plist_t strnode, const char* substr)
{
    if (!PLIST_IS_STRING(strnode)) {
        return 0;
    }
    plist_data_t data = plist_get_data(strnode);
    return (strstr(data->strval, substr) != NULL);
}

PLIST_API int plist_key_val_compare(plist_t keynode, const char* cmpval)
{
    if (!PLIST_IS_KEY(keynode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(keynode);
    return strcmp(data->strval, cmpval);
}

PLIST_API int plist_key_val_compare_with_size(plist_t keynode, const char* cmpval, size_t n)
{
    if (!PLIST_IS_KEY(keynode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(keynode);
    return strncmp(data->strval, cmpval, n);
}

PLIST_API int plist_key_val_contains(plist_t keynode, const char* substr)
{
    if (!PLIST_IS_KEY(keynode)) {
        return 0;
    }
    plist_data_t data = plist_get_data(keynode);
    return (strstr(data->strval, substr) != NULL);
}

PLIST_API int plist_data_val_compare(plist_t datanode, const uint8_t* cmpval, size_t n)
{
    if (!PLIST_IS_DATA(datanode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(datanode);
    if (data->length < n) {
        return -1;
    } else if (data->length > n) {
        return 1;
    }
    return memcmp(data->buff, cmpval, n);
}

PLIST_API int plist_data_val_compare_with_size(plist_t datanode, const uint8_t* cmpval, size_t n)
{
    if (!PLIST_IS_DATA(datanode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(datanode);
    if (data->length < n) {
        return -1;
    }
    return memcmp(data->buff, cmpval, n);
}

PLIST_API int plist_data_val_contains(plist_t datanode, const uint8_t* cmpval, size_t n)
{
    if (!PLIST_IS_DATA(datanode)) {
        return -1;
    }
    plist_data_t data = plist_get_data(datanode);
    return (memmem(data->buff, data->length, cmpval, n) != NULL);
}
