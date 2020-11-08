/*
 * base64.c
 * base64 encode/decode implementation
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
#include "base64.h"

static const char base64_str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_pad = '=';

static const signed char base64_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

size_t base64encode(char *outbuf, const unsigned char *buf, size_t size)
{
	if (!outbuf || !buf || (size <= 0)) {
		return 0;
	}

	size_t n = 0;
	size_t m = 0;
	unsigned char input[3];
	unsigned int output[4];
	while (n < size) {
		input[0] = buf[n];
		input[1] = (n+1 < size) ? buf[n+1] : 0;
		input[2] = (n+2 < size) ? buf[n+2] : 0;
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 3) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 15) << 2) + (input[2] >> 6);
		output[3] = input[2] & 63;
		outbuf[m++] = base64_str[(int)output[0]];
		outbuf[m++] = base64_str[(int)output[1]];
		outbuf[m++] = (n+1 < size) ? base64_str[(int)output[2]] : base64_pad;
		outbuf[m++] = (n+2 < size) ? base64_str[(int)output[3]] : base64_pad;
		n+=3;
	}
	outbuf[m] = 0; // 0-termination!
	return m;
}

unsigned char *base64decode(const char *buf, size_t *size)
{
	if (!buf || !size) return NULL;
	size_t len = (*size > 0) ? *size : strlen(buf);
	if (len <= 0) return NULL;
	unsigned char *outbuf = (unsigned char*)malloc((len/4)*3+3);
	const char *ptr = buf;
	int p = 0;
	int wv, w1, w2, w3, w4;
	int tmpval[4];
	int tmpcnt = 0;

	do {
		while (ptr < buf+len && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
			ptr++;
		}
		if (*ptr == '\0' || ptr >= buf+len) {
			break;
		}
		if ((wv = base64_table[(int)(unsigned char)*ptr++]) == -1) {
			continue;
		}
		tmpval[tmpcnt++] = wv;
		if (tmpcnt == 4) {
			tmpcnt = 0;
			w1 = tmpval[0];
			w2 = tmpval[1];
			w3 = tmpval[2];
			w4 = tmpval[3];

			if (w1 >= 0 && w2 >= 0) {
				outbuf[p++] = (unsigned char)(((w1 << 2) + (w2 >> 4)) & 0xFF);
			}
			if (w2 >= 0 && w3 >= 0) {
				outbuf[p++] = (unsigned char)(((w2 << 4) + (w3 >> 2)) & 0xFF);
			}
			if (w3 >= 0 && w4 >= 0) {
				outbuf[p++] = (unsigned char)(((w3 << 6) + w4) & 0xFF);
			}
		}
	} while (1);

	outbuf[p] = 0;
	*size = p;
	return outbuf;
}
