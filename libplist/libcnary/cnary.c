/*
 * cnary.c
 *
 *  Created on: Mar 9, 2011
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

#include "node.h"

int main(int argc, char* argv[]) {
	puts("Creating root node");
	node_t* root = node_create(NULL, NULL);

	puts("Creating child 1 node");
	node_t* one = node_create(root, NULL);
	puts("Creating child 2 node");
	node_t* two = node_create(root, NULL);

	puts("Creating child 3 node");
	node_t* three = node_create(one, NULL);

	puts("Debugging root node");
	node_debug(root);

	puts("Destroying root node");
	node_destroy(root);
	return 0;
}
