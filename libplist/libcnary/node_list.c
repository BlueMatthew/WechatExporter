/*
 * node_list.c
 *
 *  Created on: Mar 8, 2011
 *      Author: posixninja
 *
 * Copyright (c) 2011 Joshua Hill. All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "node_list.h"

void node_list_destroy(node_list_t* list) {
	free(list);
}

node_list_t* node_list_create() {
	node_list_t* list = (node_list_t*) malloc(sizeof(node_list_t));
	if(list == NULL) {
		return NULL;
	}
	memset(list, '\0', sizeof(node_list_t));

	// Initialize structure
	list->begin = NULL;
	list->end = NULL;
	list->count = 0;
	return list;
}

int node_list_add(node_list_t* list, node_t* node) {
	if (!list || !node) return -1;

	// Find the last element in the list
	node_t* last = list->end;

	// Setup our new node as the new last element
	node->next = NULL;
	node->prev = last;

	// Set the next element of our old "last" element
	if (last) {
		// but only if the node list is not empty
		last->next = node;
	} else {
		// otherwise this is the start of the list
		list->begin = node;
	}

	// Set the lists prev to the new last element
	list->end = node;

	// Increment our node count for this list
	list->count++;
	return 0;
}

int node_list_insert(node_list_t* list, unsigned int node_index, node_t* node) {
	if (!list || !node) return -1;
	if (node_index >= list->count) {
		return node_list_add(list, node);
	}

	// Get the first element in the list
	node_t* cur = list->begin;

	unsigned int pos = 0;
	node_t* prev = NULL;

	if (node_index > 0) {
		while (pos < node_index) {
			prev = cur;
			cur = cur->next;
			pos++;
		}
	}

	if (prev) {
		// Set previous node
		node->prev = prev;
		// Set next node of our new node to next node of the previous node
		node->next = prev->next;
		// Set next node of previous node to our new node
		prev->next = node;
	} else {
		node->prev = NULL;
		// get old first element in list
		node->next = list->begin;
		// set new node as first element in list
		list->begin = node;
	}

	if (node->next == NULL) {
		// Set the lists prev to the new last element
		list->end = node;
	} else {
		// set prev of the new next element to our node
		node->next->prev = node;
	}

	// Increment our node count for this list
	list->count++;
	return 0;
}

int node_list_remove(node_list_t* list, node_t* node) {
	if (!list || !node) return -1;
	if (list->count == 0) return -1;

	int node_index = 0;
	node_t* n;
	for (n = list->begin; n; n = n->next) {
		if (node == n) {
			node_t* newnode = node->next;
			if (node->prev) {
				node->prev->next = newnode;
				if (newnode) {
					newnode->prev = node->prev;
				} else {
					// last element in the list
					list->end = node->prev;
				}
			} else {
				// we just removed the first element
				if (newnode) {
					newnode->prev = NULL;
				} else {
					list->end = NULL;
				}
				list->begin = newnode;
			}
			list->count--;
			return node_index;
		}
		node_index++;
	}
	return -1;
}

