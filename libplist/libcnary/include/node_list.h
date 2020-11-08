/*
 * node_list.h
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

#ifndef NODE_LIST_H_
#define NODE_LIST_H_

struct node_t;

// This class implements the list_t abstract class
typedef struct node_list_t {
	// list_t members
	struct node_t* begin;
	struct node_t* end;

	// node_list_t members
	unsigned int count;

} node_list_t;

void node_list_destroy(struct node_list_t* list);
struct node_list_t* node_list_create();

int node_list_add(node_list_t* list, node_t* node);
int node_list_insert(node_list_t* list, unsigned int index, node_t* node);
int node_list_remove(node_list_t* list, node_t* node);

#endif /* NODE_LIST_H_ */
