/*
 * node.h
 *
 *  Created on: Mar 7, 2011
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

#ifndef NODE_H_
#define NODE_H_

#include "object.h"

#define NODE_TYPE 1;

struct node_list_t;

// This class implements the abstract iterator class
typedef struct node_t {
	// Super class
	struct node_t* next;
	struct node_t* prev;
	unsigned int count;

	// Local Members
	void *data;
	struct node_t* parent;
	struct node_list_t* children;
} node_t;

void node_destroy(struct node_t* node);
struct node_t* node_create(struct node_t* parent, void* data);

int node_attach(struct node_t* parent, struct node_t* child);
int node_detach(struct node_t* parent, struct node_t* child);
int node_insert(struct node_t* parent, unsigned int index, struct node_t* child);

unsigned int node_n_children(struct node_t* node);
node_t* node_nth_child(struct node_t* node, unsigned int n);
node_t* node_first_child(struct node_t* node);
node_t* node_prev_sibling(struct node_t* node);
node_t* node_next_sibling(struct node_t* node);
int node_child_position(struct node_t* parent, node_t* child);

typedef void* (*copy_func_t)(const void *src);
node_t* node_copy_deep(node_t* node, copy_func_t copy_func);

void node_debug(struct node_t* node);

#endif /* NODE_H_ */
