/*
 * xplist.c
 * XML plist implementation
 *
 * Copyright (c) 2010-2017 Nikias Bassen All Rights Reserved.
 * Copyright (c) 2010-2015 Martin Szulecki All Rights Reserved.
 * Copyright (c) 2008 Jonathan Beck All Rights Reserved.
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

#ifdef HAVE_STRPTIME
#define _XOPEN_SOURCE 600
#endif

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include <node.h>
#include <node_list.h>

#include "plist.h"
#include "base64.h"
#include "strbuf.h"
#include "time64.h"

#define XPLIST_KEY	"key"
#define XPLIST_KEY_LEN 3
#define XPLIST_FALSE	"false"
#define XPLIST_FALSE_LEN 5
#define XPLIST_TRUE	"true"
#define XPLIST_TRUE_LEN 4
#define XPLIST_INT	"integer"
#define XPLIST_INT_LEN 7
#define XPLIST_REAL	"real"
#define XPLIST_REAL_LEN 4
#define XPLIST_DATE	"date"
#define XPLIST_DATE_LEN 4
#define XPLIST_DATA	"data"
#define XPLIST_DATA_LEN 4
#define XPLIST_STRING	"string"
#define XPLIST_STRING_LEN 6
#define XPLIST_ARRAY	"array"
#define XPLIST_ARRAY_LEN 5
#define XPLIST_DICT	"dict"
#define XPLIST_DICT_LEN 4

#define MAC_EPOCH 978307200

#define MAX_DATA_BYTES_PER_LINE(__i) (((76 - (__i << 3)) >> 2) * 3)

static const char XML_PLIST_PROLOG[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n";
static const char XML_PLIST_EPILOG[] = "</plist>\n";

#ifdef DEBUG
static int plist_xml_debug = 0;
#define PLIST_XML_ERR(...) if (plist_xml_debug) { fprintf(stderr, "libplist[xmlparser] ERROR: " __VA_ARGS__); }
#else
#define PLIST_XML_ERR(...)
#endif

void plist_xml_init(void)
{
    /* init XML stuff */
#ifdef DEBUG
    char *env_debug = getenv("PLIST_XML_DEBUG");
    if (env_debug && !strcmp(env_debug, "1")) {
        plist_xml_debug = 1;
    }
#endif
}

void plist_xml_deinit(void)
{
    /* deinit XML stuff */
}

static size_t dtostr(char *buf, size_t bufsize, double realval)
{
    size_t len = 0;
    if (isnan(realval)) {
        len = snprintf(buf, bufsize, "nan");
    } else if (isinf(realval)) {
        len = snprintf(buf, bufsize, "%cinfinity", (realval > 0.0) ? '+' : '-');
    } else if (realval == 0.0f) {
        len = snprintf(buf, bufsize, "0.0");
    } else {
        size_t i = 0;
        len = snprintf(buf, bufsize, "%.*g", 17, realval);
        for (i = 0; i < len; i++) {
            if (buf[i] == ',') {
                buf[i] = '.';
                break;
            } else if (buf[i] == '.') {
                break;
            }
        }
    }
    return len;
}

static void node_to_xml(node_t* node, bytearray_t **outbuf, uint32_t depth)
{
    plist_data_t node_data = NULL;

    char isStruct = FALSE;
    char tagOpen = FALSE;

    const char *tag = NULL;
    size_t tag_len = 0;
    char *val = NULL;
    size_t val_len = 0;

    uint32_t i = 0;

    if (!node)
        return;

    node_data = plist_get_data(node);

    switch (node_data->type)
    {
    case PLIST_BOOLEAN:
    {
        if (node_data->boolval) {
            tag = XPLIST_TRUE;
            tag_len = XPLIST_TRUE_LEN;
        } else {
            tag = XPLIST_FALSE;
            tag_len = XPLIST_FALSE_LEN;
        }
    }
    break;

    case PLIST_UINT:
        tag = XPLIST_INT;
        tag_len = XPLIST_INT_LEN;
        val = (char*)malloc(64);
        if (node_data->length == 16) {
            val_len = snprintf(val, 64, "%" PRIu64, node_data->intval);
        } else {
            val_len = snprintf(val, 64, "%" PRIi64, node_data->intval);
        }
        break;

    case PLIST_REAL:
        tag = XPLIST_REAL;
        tag_len = XPLIST_REAL_LEN;
        val = (char*)malloc(64);
        val_len = dtostr(val, 64, node_data->realval);
        break;

    case PLIST_STRING:
        tag = XPLIST_STRING;
        tag_len = XPLIST_STRING_LEN;
        /* contents processed directly below */
        break;

    case PLIST_KEY:
        tag = XPLIST_KEY;
        tag_len = XPLIST_KEY_LEN;
        /* contents processed directly below */
        break;

    case PLIST_DATA:
        tag = XPLIST_DATA;
        tag_len = XPLIST_DATA_LEN;
        /* contents processed directly below */
        break;
    case PLIST_ARRAY:
        tag = XPLIST_ARRAY;
        tag_len = XPLIST_ARRAY_LEN;
        isStruct = (node->children) ? TRUE : FALSE;
        break;
    case PLIST_DICT:
        tag = XPLIST_DICT;
        tag_len = XPLIST_DICT_LEN;
        isStruct = (node->children) ? TRUE : FALSE;
        break;
    case PLIST_DATE:
        tag = XPLIST_DATE;
        tag_len = XPLIST_DATE_LEN;
        {
            Time64_T timev = (Time64_T)node_data->realval + MAC_EPOCH;
            struct TM _btime;
            struct TM *btime = gmtime64_r(&timev, &_btime);
            if (btime) {
                val = (char*)malloc(24);
                memset(val, 0, 24);
                struct tm _tmcopy;
                copy_TM64_to_tm(btime, &_tmcopy);
                val_len = strftime(val, 24, "%Y-%m-%dT%H:%M:%SZ", &_tmcopy);
                if (val_len <= 0) {
                    free (val);
                    val = NULL;
                }
            }
        }
        break;
    case PLIST_UID:
        tag = XPLIST_DICT;
        tag_len = XPLIST_DICT_LEN;
        val = (char*)malloc(64);
        if (node_data->length == 16) {
            val_len = snprintf(val, 64, "%" PRIu64, node_data->intval);
        } else {
            val_len = snprintf(val, 64, "%" PRIi64, node_data->intval);
        }
        break;
    default:
        break;
    }

    for (i = 0; i < depth; i++) {
        str_buf_append(*outbuf, "\t", 1);
    }

    /* append tag */
    str_buf_append(*outbuf, "<", 1);
    str_buf_append(*outbuf, tag, tag_len);
    if (node_data->type == PLIST_STRING || node_data->type == PLIST_KEY) {
        size_t j;
        size_t len;
        off_t start = 0;
        off_t cur = 0;

        str_buf_append(*outbuf, ">", 1);
        tagOpen = TRUE;

        /* make sure we convert the following predefined xml entities */
        /* < = &lt; > = &gt; & = &amp; */
        len = node_data->length;
        for (j = 0; j < len; j++) {
            switch (node_data->strval[j]) {
            case '<':
                str_buf_append(*outbuf, node_data->strval + start, cur - start);
                str_buf_append(*outbuf, "&lt;", 4);
                start = cur+1;
                break;
            case '>':
                str_buf_append(*outbuf, node_data->strval + start, cur - start);
                str_buf_append(*outbuf, "&gt;", 4);
                start = cur+1;
                break;
            case '&':
                str_buf_append(*outbuf, node_data->strval + start, cur - start);
                str_buf_append(*outbuf, "&amp;", 5);
                start = cur+1;
                break;
            default:
                break;
            }
            cur++;
        }
        str_buf_append(*outbuf, node_data->strval + start, cur - start);
    } else if (node_data->type == PLIST_DATA) {
        str_buf_append(*outbuf, ">", 1);
        tagOpen = TRUE;
        str_buf_append(*outbuf, "\n", 1);
        if (node_data->length > 0) {
            uint32_t j = 0;
            uint32_t indent = (depth > 8) ? 8 : depth;
            uint32_t maxread = MAX_DATA_BYTES_PER_LINE(indent);
            size_t count = 0;
            size_t amount = (node_data->length / 3 * 4) + 4 + (((node_data->length / maxread) + 1) * (indent+1));
            if ((*outbuf)->len + amount > (*outbuf)->capacity) {
                str_buf_grow(*outbuf, amount);
            }
            while (j < node_data->length) {
                for (i = 0; i < indent; i++) {
                    str_buf_append(*outbuf, "\t", 1);
                }
                count = (node_data->length-j < maxread) ? node_data->length-j : maxread;
                assert((*outbuf)->len + count < (*outbuf)->capacity);
                (*outbuf)->len += base64encode((char*)(*outbuf)->data + (*outbuf)->len, node_data->buff + j, count);
                str_buf_append(*outbuf, "\n", 1);
                j+=count;
            }
        }
        for (i = 0; i < depth; i++) {
            str_buf_append(*outbuf, "\t", 1);
        }
    } else if (node_data->type == PLIST_UID) {
        /* special case for UID nodes: create a DICT */
        str_buf_append(*outbuf, ">", 1);
        tagOpen = TRUE;
        str_buf_append(*outbuf, "\n", 1);

        /* add CF$UID key */
        for (i = 0; i < depth+1; i++) {
            str_buf_append(*outbuf, "\t", 1);
        }
        str_buf_append(*outbuf, "<key>CF$UID</key>", 17);
        str_buf_append(*outbuf, "\n", 1);

        /* add UID value */
        for (i = 0; i < depth+1; i++) {
            str_buf_append(*outbuf, "\t", 1);
        }
        str_buf_append(*outbuf, "<integer>", 9);
        str_buf_append(*outbuf, val, val_len);
        str_buf_append(*outbuf, "</integer>", 10);
        str_buf_append(*outbuf, "\n", 1);

        for (i = 0; i < depth; i++) {
            str_buf_append(*outbuf, "\t", 1);
        }
    } else if (val) {
        str_buf_append(*outbuf, ">", 1);
        tagOpen = TRUE;
        str_buf_append(*outbuf, val, val_len);
    } else if (isStruct) {
        tagOpen = TRUE;
        str_buf_append(*outbuf, ">", 1);
    } else {
        tagOpen = FALSE;
        str_buf_append(*outbuf, "/>", 2);
    }
    free(val);

    if (isStruct) {
        /* add newline for structured types */
        str_buf_append(*outbuf, "\n", 1);

        /* add child nodes */
        if (node_data->type == PLIST_DICT && node->children) {
            assert((node->children->count % 2) == 0);
        }
        node_t *ch;
        for (ch = node_first_child(node); ch; ch = node_next_sibling(ch)) {
            node_to_xml(ch, outbuf, depth+1);
        }

        /* fix indent for structured types */
        for (i = 0; i < depth; i++) {
            str_buf_append(*outbuf, "\t", 1);
        }
    }

    if (tagOpen) {
        /* add closing tag */
        str_buf_append(*outbuf, "</", 2);
        str_buf_append(*outbuf, tag, tag_len);
        str_buf_append(*outbuf, ">", 1);
    }
    str_buf_append(*outbuf, "\n", 1);
}

static void parse_date(const char *strval, struct TM *btime)
{
    if (!btime) return;
    memset(btime, 0, sizeof(struct tm));
    if (!strval) return;
#ifdef HAVE_STRPTIME
    strptime((char*)strval, "%Y-%m-%dT%H:%M:%SZ", btime);
#else
#ifdef USE_TM64
    #define PLIST_SSCANF_FORMAT "%lld-%d-%dT%d:%d:%dZ"
#else
    #define PLIST_SSCANF_FORMAT "%d-%d-%dT%d:%d:%dZ"
#endif
    sscanf(strval, PLIST_SSCANF_FORMAT, &btime->tm_year, &btime->tm_mon, &btime->tm_mday, &btime->tm_hour, &btime->tm_min, &btime->tm_sec);
    btime->tm_year-=1900;
    btime->tm_mon--;
#endif
    btime->tm_isdst=0;
}

#define PO10i_LIMIT (INT64_MAX/10)

/* based on https://stackoverflow.com/a/4143288 */
static int num_digits_i(int64_t i)
{
    int n;
    int64_t po10;
    n=1;
    if (i < 0) {
        i = -i;
        n++;
    }
    po10=10;
    while (i>=po10) {
        n++;
        if (po10 > PO10i_LIMIT) break;
        po10*=10;
    }
    return n;
}

#define PO10u_LIMIT (UINT64_MAX/10)

/* based on https://stackoverflow.com/a/4143288 */
static int num_digits_u(uint64_t i)
{
    int n;
    uint64_t po10;
    n=1;
    po10=10;
    while (i>=po10) {
        n++;
        if (po10 > PO10u_LIMIT) break;
        po10*=10;
    }
    return n;
}

static void node_estimate_size(node_t *node, uint64_t *size, uint32_t depth)
{
    plist_data_t data;
    if (!node) {
        return;
    }
    data = plist_get_data(node);
    if (node->children) {
        node_t *ch;
        for (ch = node_first_child(node); ch; ch = node_next_sibling(ch)) {
            node_estimate_size(ch, size, depth + 1);
        }
        switch (data->type) {
        case PLIST_DICT:
            *size += (XPLIST_DICT_LEN << 1) + 7;
            break;
        case PLIST_ARRAY:
            *size += (XPLIST_ARRAY_LEN << 1) + 7;
            break;
        default:
            break;
	}
        *size += (depth << 1);
    } else {
        uint32_t indent = (depth > 8) ? 8 : depth;
        switch (data->type) {
        case PLIST_DATA: {
            uint32_t req_lines = (data->length / MAX_DATA_BYTES_PER_LINE(indent)) + 1;
            uint32_t b64len = data->length + (data->length / 3);
            b64len += b64len % 4;
            *size += b64len;
            *size += (XPLIST_DATA_LEN << 1) + 5 + (indent+1) * (req_lines+1) + 1;
        }   break;
        case PLIST_STRING:
            *size += data->length;
            *size += (XPLIST_STRING_LEN << 1) + 6;
            break;
        case PLIST_KEY:
            *size += data->length;
            *size += (XPLIST_KEY_LEN << 1) + 6;
            break;
        case PLIST_UINT:
            if (data->length == 16) {
                *size += num_digits_u(data->intval);
            } else {
                *size += num_digits_i((int64_t)data->intval);
            }
            *size += (XPLIST_INT_LEN << 1) + 6;
            break;
        case PLIST_REAL:
            *size += num_digits_i((int64_t)data->realval) + 7;
            *size += (XPLIST_REAL_LEN << 1) + 6;
            break;
        case PLIST_DATE:
            *size += 20; /* YYYY-MM-DDThh:mm:ssZ */
            *size += (XPLIST_DATE_LEN << 1) + 6;
            break;
        case PLIST_BOOLEAN:
            *size += ((data->boolval) ? XPLIST_TRUE_LEN : XPLIST_FALSE_LEN) + 4;
            break;
        case PLIST_DICT:
            *size += XPLIST_DICT_LEN + 4; /* <dict/> */
            break;
        case PLIST_ARRAY:
            *size += XPLIST_ARRAY_LEN + 4; /* <array/> */
            break;
        case PLIST_UID:
            *size += num_digits_i((int64_t)data->intval);
            *size += (XPLIST_DICT_LEN << 1) + 7;
            *size += indent + ((indent+1) << 1);
            *size += 18; /* <key>CF$UID</key> */
            *size += (XPLIST_INT_LEN << 1) + 6;
            break;
        default:
            break;
        }
        *size += indent;
    }
}

PLIST_API void plist_to_xml(plist_t plist, char **plist_xml, uint32_t * length)
{
    uint64_t size = 0;
    node_estimate_size((node_t*)plist, &size, 0);
    size += sizeof(XML_PLIST_PROLOG) + sizeof(XML_PLIST_EPILOG) - 1;

    strbuf_t *outbuf = str_buf_new(size);

    str_buf_append(outbuf, XML_PLIST_PROLOG, sizeof(XML_PLIST_PROLOG)-1);

    node_to_xml((node_t*)plist, &outbuf, 0);

    str_buf_append(outbuf, XML_PLIST_EPILOG, sizeof(XML_PLIST_EPILOG));

    *plist_xml = (char*)outbuf->data;
    *length = outbuf->len - 1;

    outbuf->data = NULL;
    str_buf_free(outbuf);
}

PLIST_API void plist_to_xml_free(char *plist_xml)
{
    free(plist_xml);
}

struct _parse_ctx {
    const char *pos;
    const char *end;
    int err;
};
typedef struct _parse_ctx* parse_ctx;

static void parse_skip_ws(parse_ctx ctx)
{
    while (ctx->pos < ctx->end && ((*(ctx->pos) == ' ') || (*(ctx->pos) == '\t') || (*(ctx->pos) == '\r') || (*(ctx->pos) == '\n'))) {
        ctx->pos++;
    }
}

static void find_char(parse_ctx ctx, char c, int skip_quotes)
{
    while (ctx->pos < ctx->end && (*(ctx->pos) != c)) {
        if (skip_quotes && (c != '"') && (*(ctx->pos) == '"')) {
            ctx->pos++;
            find_char(ctx, '"', 0);
            if (ctx->pos >= ctx->end) {
                PLIST_XML_ERR("EOF while looking for matching double quote\n");
                return;
            }
            if (*(ctx->pos) != '"') {
                PLIST_XML_ERR("Unmatched double quote\n");
                return;
            }
        }
        ctx->pos++;
    }
}

static void find_str(parse_ctx ctx, const char *str, size_t len, int skip_quotes)
{
    while (ctx->pos < (ctx->end - len)) {
        if (!strncmp(ctx->pos, str, len)) {
            break;
        }
        if (skip_quotes && (*(ctx->pos) == '"')) {
            ctx->pos++;
            find_char(ctx, '"', 0);
            if (ctx->pos >= ctx->end) {
                PLIST_XML_ERR("EOF while looking for matching double quote\n");
                return;
            }
            if (*(ctx->pos) != '"') {
                PLIST_XML_ERR("Unmatched double quote\n");
                return;
            }
        }
        ctx->pos++;
    }
}

static void find_next(parse_ctx ctx, const char *nextchars, int numchars, int skip_quotes)
{
    int i = 0;
    while (ctx->pos < ctx->end) {
        if (skip_quotes && (*(ctx->pos) == '"')) {
            ctx->pos++;
            find_char(ctx, '"', 0);
            if (ctx->pos >= ctx->end) {
                PLIST_XML_ERR("EOF while looking for matching double quote\n");
                return;
            }
            if (*(ctx->pos) != '"') {
                PLIST_XML_ERR("Unmatched double quote\n");
                return;
            }
        }
        for (i = 0; i < numchars; i++) {
            if (*(ctx->pos) == nextchars[i]) {
                return;
            }
        }
        ctx->pos++;
    }
}

typedef struct {
    const char *begin;
    size_t length;
    int is_cdata;
    void *next;
} text_part_t;

static text_part_t* text_part_init(text_part_t* part, const char *begin, size_t length, int is_cdata)
{
    part->begin = begin;
    part->length = length;
    part->is_cdata = is_cdata;
    part->next = NULL;
    return part;
}

static void text_parts_free(text_part_t *tp)
{
    while (tp) {
        text_part_t *tmp = tp;
        tp = (text_part_t *)tp->next;
        free(tmp);
    }
}

static text_part_t* text_part_append(text_part_t* parts, const char *begin, size_t length, int is_cdata)
{
    text_part_t* newpart = (text_part_t *)malloc(sizeof(text_part_t));
    assert(newpart);
    parts->next = text_part_init(newpart, begin, length, is_cdata);
    return newpart;
}

static text_part_t* get_text_parts(parse_ctx ctx, const char* tag, size_t tag_len, int skip_ws, text_part_t *parts)
{
    const char *p = NULL;
    const char *q = NULL;
    text_part_t *last = NULL;

    if (skip_ws) {
        parse_skip_ws(ctx);
    }
    do {
        p = ctx->pos;
        find_char(ctx, '<', 0);
        if (ctx->pos >= ctx->end || *ctx->pos != '<') {
            PLIST_XML_ERR("EOF while looking for closing tag\n");
            ctx->err++;
            return NULL;
        }
        q = ctx->pos;
        ctx->pos++;
        if (ctx->pos >= ctx->end) {
            PLIST_XML_ERR("EOF while parsing '%s'\n", p);
            ctx->err++;
            return NULL;
        }
        if (*ctx->pos == '!') {
            ctx->pos++;
            if (ctx->pos >= ctx->end-1) {
                PLIST_XML_ERR("EOF while parsing <! special tag\n");
                ctx->err++;
                return NULL;
            }
            if (*ctx->pos == '-' && *(ctx->pos+1) == '-') {
                if (last) {
                    last = text_part_append(last, p, q-p, 0);
                } else if (parts) {
                    last = text_part_init(parts, p, q-p, 0);
                }
                ctx->pos += 2;
                find_str(ctx, "-->", 3, 0);
                if (ctx->pos > ctx->end-3 || strncmp(ctx->pos, "-->", 3) != 0) {
                    PLIST_XML_ERR("EOF while looking for end of comment\n");
                    ctx->err++;
                    return NULL;
                }
                ctx->pos += 3;
            } else if (*ctx->pos == '[') {
                ctx->pos++;
                if (ctx->pos >= ctx->end - 8) {
                    PLIST_XML_ERR("EOF while parsing <[ tag\n");
                    ctx->err++;
                    return NULL;
                }
                if (strncmp(ctx->pos, "CDATA[", 6) == 0) {
                    if (q-p > 0) {
                        if (last) {
                            last = text_part_append(last, p, q-p, 0);
                        } else if (parts) {
                            last = text_part_init(parts, p, q-p, 0);
                        }
                    }
                    ctx->pos+=6;
                    p = ctx->pos;
                    find_str(ctx, "]]>", 3, 0);
                    if (ctx->pos > ctx->end-3 || strncmp(ctx->pos, "]]>", 3) != 0) {
                        PLIST_XML_ERR("EOF while looking for end of CDATA block\n");
                        ctx->err++;
                        return NULL;
                    }
                    q = ctx->pos;
                    if (last) {
                        last = text_part_append(last, p, q-p, 1);
                    } else if (parts) {
                        last = text_part_init(parts, p, q-p, 1);
                    }
                    ctx->pos += 3;
                } else {
                    p = ctx->pos;
                    find_next(ctx, " \r\n\t>", 5, 1);
                    PLIST_XML_ERR("Invalid special tag <[%.*s> encountered inside <%s> tag\n", (int)(ctx->pos - p), p, tag);
                    ctx->err++;
                    return NULL;
                }
            } else {
                p = ctx->pos;
                find_next(ctx, " \r\n\t>", 5, 1);
                PLIST_XML_ERR("Invalid special tag <!%.*s> encountered inside <%s> tag\n", (int)(ctx->pos - p), p, tag);
                ctx->err++;
                return NULL;
            }
        } else if (*ctx->pos == '/') {
            break;
        } else {
            p = ctx->pos;
            find_next(ctx, " \r\n\t>", 5, 1);
            PLIST_XML_ERR("Invalid tag <%.*s> encountered inside <%s> tag\n", (int)(ctx->pos - p), p, tag);
            ctx->err++;
            return NULL;
        }
    } while (1);
    ctx->pos++;
    if (ctx->pos >= ctx->end-tag_len || strncmp(ctx->pos, tag, tag_len)) {
        PLIST_XML_ERR("EOF or end tag mismatch\n");
        ctx->err++;
        return NULL;
    }
    ctx->pos+=tag_len;
    parse_skip_ws(ctx);
    if (ctx->pos >= ctx->end) {
        PLIST_XML_ERR("EOF while parsing closing tag\n");
        ctx->err++;
        return NULL;
    } else if (*ctx->pos != '>') {
        PLIST_XML_ERR("Invalid closing tag; expected '>', found '%c'\n", *ctx->pos);
        ctx->err++;
        return NULL;
    }
    ctx->pos++;

    if (q-p > 0) {
        if (last) {
            last = text_part_append(last, p, q-p, 0);
        } else if (parts) {
            last = text_part_init(parts, p, q-p, 0);
        }
    }
    return parts;
}

static int unescape_entities(char *str, size_t *length)
{
    size_t i = 0;
    size_t len = *length;
    while (len > 0 && i < len-1) {
        if (str[i] == '&') {
            char *entp = str + i + 1;
            while (i < len && str[i] != ';') {
                i++;
            }
            if (i >= len) {
                PLIST_XML_ERR("Invalid entity sequence encountered (missing terminating ';')\n");
                return -1;
            }
            if (str+i >= entp+1) {
                int entlen = str+i - entp;
                int bytelen = 1;
                if (!strncmp(entp, "amp", 3)) {
                    /* the '&' is already there */
                } else if (!strncmp(entp, "apos", 4)) {
                    *(entp-1) = '\'';
                } else if (!strncmp(entp, "quot", 4)) {
                    *(entp-1) = '"';
                } else if (!strncmp(entp, "lt", 2)) {
                    *(entp-1) = '<';
                } else if (!strncmp(entp, "gt", 2)) {
                    *(entp-1) = '>';
                } else if (*entp == '#') {
                    /* numerical  character reference */
                    uint64_t val = 0;
                    char* ep = NULL;
                    if (entlen > 8) {
                        PLIST_XML_ERR("Invalid numerical character reference encountered, sequence too long: &%.*s;\n", entlen, entp);
                        return -1;
                    }
                    if (*(entp+1) == 'x' || *(entp+1) == 'X') {
                        if (entlen < 3) {
                            PLIST_XML_ERR("Invalid numerical character reference encountered, sequence too short: &%.*s;\n", entlen, entp);
                            return -1;
                        }
                        val = strtoull(entp+2, &ep, 16);
                    } else {
                        if (entlen < 2) {
                            PLIST_XML_ERR("Invalid numerical character reference encountered, sequence too short: &%.*s;\n", entlen, entp);
                            return -1;
                        }
                        val = strtoull(entp+1, &ep, 10);
                    }
                    if (val == 0 || val > 0x10FFFF || ep-entp != entlen) {
                        PLIST_XML_ERR("Invalid numerical character reference found: &%.*s;\n", entlen, entp);
                        return -1;
                    }
                    /* convert to UTF8 */
                    if (val >= 0x10000) {
                        /* four bytes */
                        *(entp-1) = (char)(0xF0 + ((val >> 18) & 0x7));
                        *(entp+0) = (char)(0x80 + ((val >> 12) & 0x3F));
                        *(entp+1) = (char)(0x80 + ((val >> 6) & 0x3F));
                        *(entp+2) = (char)(0x80 + (val & 0x3F));
                        entp+=3;
                        bytelen = 4;
                    } else if (val >= 0x800) {
                        /* three bytes */
                        *(entp-1) = (char)(0xE0 + ((val >> 12) & 0xF));
                        *(entp+0) = (char)(0x80 + ((val >> 6) & 0x3F));
                        *(entp+1) = (char)(0x80 + (val & 0x3F));
                        entp+=2;
                        bytelen = 3;
                    } else if (val >= 0x80) {
                        /* two bytes */
                        *(entp-1) = (char)(0xC0 + ((val >> 6) & 0x1F));
                        *(entp+0) = (char)(0x80 + (val & 0x3F));
                        entp++;
                        bytelen = 2;
                    } else {
                        /* one byte */
                        *(entp-1) = (char)(val & 0x7F);
                    }
                } else {
                    PLIST_XML_ERR("Invalid entity encountered: &%.*s;\n", entlen, entp);
                    return -1;
                }
                memmove(entp, str+i+1, len - i);
                i -= entlen+1 - bytelen;
                len -= entlen+2 - bytelen;
                continue;
            } else {
                PLIST_XML_ERR("Invalid empty entity sequence &;\n");
                return -1;
            }
        }
        i++;
    }
    *length = len;
    return 0;
}

static char* text_parts_get_content(text_part_t *tp, int unesc_entities, size_t *length, int *requires_free)
{
    char *str = NULL;
    size_t total_length = 0;

    if (!tp) {
        return NULL;
    }
    char *p;
    if (requires_free && !tp->next) {
        if (tp->is_cdata || !unesc_entities) {
            *requires_free = 0;
            if (length) {
                *length = tp->length;
            }
            return (char*)tp->begin;
        }
    }
    text_part_t *tmp = tp;
    while (tp && tp->begin) {
        total_length += tp->length;
        tp = (text_part_t *)tp->next;
    }
    str = (char *)malloc(total_length + 1);
    assert(str);
    p = str;
    tp = tmp;
    while (tp && tp->begin) {
        size_t len = tp->length;
        strncpy(p, tp->begin, len);
        p[len] = '\0';
        if (!tp->is_cdata && unesc_entities) {
            if (unescape_entities(p, &len) < 0) {
                free(str);
                return NULL;
            }
        }
        p += len;
        tp = (text_part_t *)tp->next;
    }
    *p = '\0';
    if (length) {
        *length = p - str;
    }
    if (requires_free) {
        *requires_free = 1;
    }
    return str;
}

static void node_from_xml(parse_ctx ctx, plist_t *plist)
{
    char *tag = NULL;
    char *keyname = NULL;
    plist_t subnode = NULL;
    const char *p = NULL;
    plist_t parent = NULL;
    int has_content = 0;

    struct node_path_item {
        const char *type;
        void *prev;
    };
    struct node_path_item* node_path = NULL;

    while (ctx->pos < ctx->end && !ctx->err) {
        parse_skip_ws(ctx);
        if (ctx->pos >= ctx->end) {
            break;
        }
        if (*ctx->pos != '<') {
            p = ctx->pos;
            find_next(ctx, " \t\r\n", 4, 0);
            PLIST_XML_ERR("Expected: opening tag, found: %.*s\n", (int)(ctx->pos - p), p);
            ctx->err++;
            goto err_out;
        }
        ctx->pos++;
        if (ctx->pos >= ctx->end) {
            PLIST_XML_ERR("EOF while parsing tag\n");
            ctx->err++;
            goto err_out;
        }

        if (*(ctx->pos) == '?') {
            find_str(ctx, "?>", 2, 1);
            if (ctx->pos > ctx->end-2) {
                PLIST_XML_ERR("EOF while looking for <? tag closing marker\n");
                ctx->err++;
                goto err_out;
            }
            if (strncmp(ctx->pos, "?>", 2)) {
                PLIST_XML_ERR("Couldn't find <? tag closing marker\n");
                ctx->err++;
                goto err_out;
            }
            ctx->pos += 2;
            continue;
        } else if (*(ctx->pos) == '!') {
            /* comment or DTD */
            if (((ctx->end - ctx->pos) > 3) && !strncmp(ctx->pos, "!--", 3)) {
                ctx->pos += 3;
                find_str(ctx,"-->", 3, 0);
                if (ctx->pos > ctx->end-3 || strncmp(ctx->pos, "-->", 3)) {
                    PLIST_XML_ERR("Couldn't find end of comment\n");
                    ctx->err++;
                    goto err_out;
                }
                ctx->pos+=3;
            } else if (((ctx->end - ctx->pos) > 8) && !strncmp(ctx->pos, "!DOCTYPE", 8)) {
                int embedded_dtd = 0;
                ctx->pos+=8;
                while (ctx->pos < ctx->end) {
                    find_next(ctx, " \t\r\n[>", 6, 1);
                    if (ctx->pos >= ctx->end) {
                        PLIST_XML_ERR("EOF while parsing !DOCTYPE\n");
                        ctx->err++;
                        goto err_out;
                    }
                    if (*ctx->pos == '[') {
                        embedded_dtd = 1;
                        break;
                    } else if (*ctx->pos == '>') {
                        /* end of DOCTYPE found already */
                        ctx->pos++;
                        break;
                    } else {
                        parse_skip_ws(ctx);
                    }
                }
                if (embedded_dtd) {
                    find_str(ctx, "]>", 2, 1);
                    if (ctx->pos > ctx->end-2 || strncmp(ctx->pos, "]>", 2)) {
                        PLIST_XML_ERR("Couldn't find end of DOCTYPE\n");
                        ctx->err++;
                        goto err_out;
                    }
                    ctx->pos += 2;
                }
            } else {
                p = ctx->pos;
                find_next(ctx, " \r\n\t>", 5, 1);
                PLIST_XML_ERR("Invalid or incomplete special tag <%.*s> encountered\n", (int)(ctx->pos - p), p);
                ctx->err++;
                goto err_out;
            }
            continue;
        } else {
            int is_empty = 0;
            int closing_tag = 0;
            p = ctx->pos;
            find_next(ctx," \r\n\t<>", 6, 0);
            if (ctx->pos >= ctx->end) {
                PLIST_XML_ERR("Unexpected EOF while parsing XML\n");
                ctx->err++;
                goto err_out;
            }
            int taglen = ctx->pos - p;
            tag = (char*)malloc(taglen + 1);
            strncpy(tag, p, taglen);
            tag[taglen] = '\0';
            if (*ctx->pos != '>') {
                find_next(ctx, "<>", 2, 1);
            }
            if (ctx->pos >= ctx->end) {
                PLIST_XML_ERR("Unexpected EOF while parsing XML\n");
                ctx->err++;
                goto err_out;
            }
            if (*ctx->pos != '>') {
                PLIST_XML_ERR("Missing '>' for tag <%s\n", tag);
                ctx->err++;
                goto err_out;
            }
            if (*(ctx->pos-1) == '/') {
                int idx = ctx->pos - p - 1;
                if (idx < taglen)
                    tag[idx] = '\0';
                is_empty = 1;
            }
            ctx->pos++;
            if (!strcmp(tag, "plist")) {
                free(tag);
                tag = NULL;
                has_content = 0;

                if (!node_path && *plist) {
                    /* we don't allow another top-level <plist> */
                    break;
                }
                if (is_empty) {
                    PLIST_XML_ERR("Empty plist tag\n");
                    ctx->err++;
                    goto err_out;
                }

                struct node_path_item *path_item = (struct node_path_item*)malloc(sizeof(struct node_path_item));
                if (!path_item) {
                    PLIST_XML_ERR("out of memory when allocating node path item\n");
                    ctx->err++;
                    goto err_out;
                }
                path_item->type = "plist";
                path_item->prev = node_path;
                node_path = path_item;

                continue;
            } else if (!strcmp(tag, "/plist")) {
                if (!has_content) {
                    PLIST_XML_ERR("encountered empty plist tag\n");
                    ctx->err++;
                    goto err_out;
                }
                if (!node_path) {
                    PLIST_XML_ERR("node path is empty while trying to match closing tag with opening tag\n");
                    ctx->err++;
                    goto err_out;
                }
                if (strcmp(node_path->type, tag+1) != 0) {
                    PLIST_XML_ERR("mismatching closing tag <%s> found for opening tag <%s>\n", tag, node_path->type);
                    ctx->err++;
                    goto err_out;
                }
                struct node_path_item *path_item = node_path;
                node_path = (struct node_path_item*)node_path->prev;
                free(path_item);

                free(tag);
                tag = NULL;

                continue;
            }

            plist_data_t data = plist_new_plist_data();
            subnode = plist_new_node(data);
            has_content = 1;

            if (!strcmp(tag, XPLIST_DICT)) {
                data->type = PLIST_DICT;
            } else if (!strcmp(tag, XPLIST_ARRAY)) {
                data->type = PLIST_ARRAY;
            } else if (!strcmp(tag, XPLIST_INT)) {
                if (!is_empty) {
                    text_part_t first_part = { NULL, 0, 0, NULL };
                    text_part_t *tp = get_text_parts(ctx, tag, taglen, 1, &first_part);
                    if (!tp) {
                        PLIST_XML_ERR("Could not parse text content for '%s' node\n", tag);
                        text_parts_free((text_part_t *)first_part.next);
                        ctx->err++;
                        goto err_out;
                    }
                    if (tp->begin) {
                        int requires_free = 0;
                        char *str_content = text_parts_get_content(tp, 0, NULL, &requires_free);
                        if (!str_content) {
                            PLIST_XML_ERR("Could not get text content for '%s' node\n", tag);
                            text_parts_free((text_part_t *)first_part.next);
                            ctx->err++;
                            goto err_out;
                        }
                        char *str = str_content;
                        int is_negative = 0;
                        if ((str[0] == '-') || (str[0] == '+')) {
                            if (str[0] == '-') {
                                is_negative = 1;
                            }
                            str++;
                        }
                        data->intval = strtoull((char*)str, NULL, 0);
                        if (is_negative || (data->intval <= INT64_MAX)) {
                            uint64_t v = data->intval;
                            if (is_negative) {
                                v = -v;
                            }
                            data->intval = v;
                            data->length = 8;
                        } else {
                            data->length = 16;
                        }
                        if (requires_free) {
                            free(str_content);
                        }
                    } else {
                        is_empty = 1;
                    }
                    text_parts_free((text_part_t *)tp->next);
                }
                if (is_empty) {
                    data->intval = 0;
                    data->length = 8;
                }
                data->type = PLIST_UINT;
            } else if (!strcmp(tag, XPLIST_REAL)) {
                if (!is_empty) {
                    text_part_t first_part = { NULL, 0, 0, NULL };
                    text_part_t *tp = get_text_parts(ctx, tag, taglen, 1, &first_part);
                    if (!tp) {
                        PLIST_XML_ERR("Could not parse text content for '%s' node\n", tag);
                        text_parts_free((text_part_t*)first_part.next);
                        ctx->err++;
                        goto err_out;
                    }
                    if (tp->begin) {
                        int requires_free = 0;
                        char *str_content = text_parts_get_content(tp, 0, NULL, &requires_free);
                        if (!str_content) {
                            PLIST_XML_ERR("Could not get text content for '%s' node\n", tag);
                            text_parts_free((text_part_t*)first_part.next);
                            ctx->err++;
                            goto err_out;
                        }
                        data->realval = atof(str_content);
                        if (requires_free) {
                            free(str_content);
                        }
                    }
                    text_parts_free((text_part_t*)tp->next);
                }
                data->type = PLIST_REAL;
                data->length = 8;
            } else if (!strcmp(tag, XPLIST_TRUE)) {
                if (!is_empty) {
                    get_text_parts(ctx, tag, taglen, 1, NULL);
                }
                data->type = PLIST_BOOLEAN;
                data->boolval = 1;
                data->length = 1;
            } else if (!strcmp(tag, XPLIST_FALSE)) {
                if (!is_empty) {
                    get_text_parts(ctx, tag, taglen, 1, NULL);
                }
                data->type = PLIST_BOOLEAN;
                data->boolval = 0;
                data->length = 1;
            } else if (!strcmp(tag, XPLIST_STRING) || !strcmp(tag, XPLIST_KEY)) {
                if (!is_empty) {
                    text_part_t first_part = { NULL, 0, 0, NULL };
                    text_part_t *tp = get_text_parts(ctx, tag, taglen, 0, &first_part);
                    char *str = NULL;
                    size_t length = 0;
                    if (!tp) {
                        PLIST_XML_ERR("Could not parse text content for '%s' node\n", tag);
                        text_parts_free((text_part_t*)first_part.next);
                        ctx->err++;
                        goto err_out;
                    }
                    str = text_parts_get_content(tp, 1, &length, NULL);
                    text_parts_free((text_part_t*)first_part.next);
                    if (!str) {
                        PLIST_XML_ERR("Could not get text content for '%s' node\n", tag);
                        ctx->err++;
                        goto err_out;
                    }
                    if (!strcmp(tag, "key") && !keyname && parent && (plist_get_node_type(parent) == PLIST_DICT)) {
                        keyname = str;
                        free(tag);
                        tag = NULL;
                        plist_free(subnode);
                        subnode = NULL;
                        continue;
                    } else {
                        data->strval = str;
                        data->length = length;
                    }
                } else {
                    data->strval = strdup("");
                    data->length = 0;
                }
                data->type = PLIST_STRING;
            } else if (!strcmp(tag, XPLIST_DATA)) {
                if (!is_empty) {
                    text_part_t first_part = { NULL, 0, 0, NULL };
                    text_part_t *tp = get_text_parts(ctx, tag, taglen, 1, &first_part);
                    if (!tp) {
                        PLIST_XML_ERR("Could not parse text content for '%s' node\n", tag);
                        text_parts_free((text_part_t*)first_part.next);
                        ctx->err++;
                        goto err_out;
                    }
                    if (tp->begin) {
                        int requires_free = 0;
                        char *str_content = text_parts_get_content(tp, 0, NULL, &requires_free);
                        if (!str_content) {
                            PLIST_XML_ERR("Could not get text content for '%s' node\n", tag);
                            text_parts_free((text_part_t*)first_part.next);
                            ctx->err++;
                            goto err_out;
                        }
                        size_t size = tp->length;
                        if (size > 0) {
                            data->buff = base64decode(str_content, &size);
                            data->length = size;
                        }

                        if (requires_free) {
                            free(str_content);
                        }
                    }
                    text_parts_free((text_part_t*)tp->next);
                }
                data->type = PLIST_DATA;
            } else if (!strcmp(tag, XPLIST_DATE)) {
                if (!is_empty) {
                    text_part_t first_part = { NULL, 0, 0, NULL };
                    text_part_t *tp = get_text_parts(ctx, tag, taglen, 1, &first_part);
                    if (!tp) {
                        PLIST_XML_ERR("Could not parse text content for '%s' node\n", tag);
                        text_parts_free((text_part_t*)first_part.next);
                        ctx->err++;
                        goto err_out;
                    }
                    Time64_T timev = 0;
                    if (tp->begin) {
                        int requires_free = 0;
                        size_t length = 0;
                        char *str_content = text_parts_get_content(tp, 0, &length, &requires_free);
                        if (!str_content) {
                            PLIST_XML_ERR("Could not get text content for '%s' node\n", tag);
                            text_parts_free((text_part_t*)first_part.next);
                            ctx->err++;
                            goto err_out;
                        }

                        if ((length >= 11) && (length < 32)) {
                            /* we need to copy here and 0-terminate because sscanf will read the entire string (whole rest of XML data) which can be huge */
                            char strval[32];
                            struct TM btime;
                            strncpy(strval, str_content, length);
                            strval[tp->length] = '\0';
                            parse_date(strval, &btime);
                            timev = timegm64(&btime);
                        } else {
                            PLIST_XML_ERR("Invalid text content in date node\n");
                        }
                        if (requires_free) {
                            free(str_content);
                        }
                    }
                    text_parts_free((text_part_t*)tp->next);
                    data->realval = (double)(timev - MAC_EPOCH);
                }
                data->length = sizeof(double);
                data->type = PLIST_DATE;
            } else if (tag[0] == '/') {
                 closing_tag = 1;
            } else {
                PLIST_XML_ERR("Unexpected tag <%s%s> encountered\n", tag, (is_empty) ? "/" : "");
                ctx->pos = ctx->end;
                ctx->err++;
                goto err_out;
            }
            if (subnode && !closing_tag) {
                if (!*plist) {
                    /* first node, make this node the parent node */
                    *plist = subnode;
                    if (data->type != PLIST_DICT && data->type != PLIST_ARRAY) {
                        /* if the first node is not a structered node, we're done */
                        subnode = NULL;
                        goto err_out;
                    }
                    parent = subnode;
                } else if (parent) {
                    switch (plist_get_node_type(parent)) {
                    case PLIST_DICT:
                        if (!keyname) {
                            PLIST_XML_ERR("missing key name while adding dict item\n");
                            ctx->err++;
                            goto err_out;
                        }
                        plist_dict_set_item(parent, keyname, subnode);
                        break;
                    case PLIST_ARRAY:
                        plist_array_append_item(parent, subnode);
                        break;
                    default:
                        /* should not happen */
                        PLIST_XML_ERR("parent is not a structured node\n");
                        ctx->err++;
                        goto err_out;
                    }
                }
                if (!is_empty && (data->type == PLIST_DICT || data->type == PLIST_ARRAY)) {
                    struct node_path_item *path_item = (struct node_path_item*)malloc(sizeof(struct node_path_item));
                    if (!path_item) {
                        PLIST_XML_ERR("out of memory when allocating node path item\n");
                        ctx->err++;
                        goto err_out;
                    }
                    path_item->type = (data->type == PLIST_DICT) ? XPLIST_DICT : XPLIST_ARRAY;
                    path_item->prev = node_path;
                    node_path = path_item;

                    parent = subnode;
                }
                subnode = NULL;
            } else if (closing_tag) {
                if (!node_path) {
                    PLIST_XML_ERR("node path is empty while trying to match closing tag with opening tag\n");
                    ctx->err++;
                    goto err_out;
                }
                if (strcmp(node_path->type, tag+1) != 0) {
                    PLIST_XML_ERR("unexpected %s found (for opening %s)\n", tag, node_path->type);
                    ctx->err++;
                    goto err_out;
                }
                struct node_path_item *path_item = node_path;
                node_path = (struct node_path_item*)node_path->prev;
                free(path_item);

                parent = ((node_t*)parent)->parent;
                if (!parent) {
                    goto err_out;
                }
            }

            free(tag);
            tag = NULL;
            free(keyname);
            keyname = NULL;
            plist_free(subnode);
            subnode = NULL;
        }
    }

    if (node_path) {
        PLIST_XML_ERR("EOF encountered while </%s> was expected\n", node_path->type);
        ctx->err++;
    }

err_out:
    free(tag);
    free(keyname);
    plist_free(subnode);

    /* clean up node_path if required */
    while (node_path) {
        struct node_path_item *path_item = node_path;
        node_path = (struct node_path_item*)path_item->prev;
        free(path_item);
    }

    if (ctx->err) {
        plist_free(*plist);
        *plist = NULL;
    }
}

PLIST_API void plist_from_xml(const char *plist_xml, uint32_t length, plist_t * plist)
{
    if (!plist_xml || (length == 0)) {
        *plist = NULL;
        return;
    }

    struct _parse_ctx ctx = { plist_xml, plist_xml + length, 0 };

    node_from_xml(&ctx, plist);
}
