/*
 * Node.cpp
 *
 * Copyright (c) 2009 Jonathan Beck All Rights Reserved.
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

#include <cstdlib>
#include <plist/Node.h>
#include <plist/Structure.h>
#include <plist/Dictionary.h>
#include <plist/Array.h>
#include <plist/Boolean.h>
#include <plist/Integer.h>
#include <plist/Real.h>
#include <plist/String.h>
#include <plist/Key.h>
#include <plist/Uid.h>
#include <plist/Data.h>
#include <plist/Date.h>

namespace PList
{

Node::Node(Node* parent) : _parent(parent)
{
}

Node::Node(plist_t node, Node* parent) : _node(node), _parent(parent)
{
}

Node::Node(plist_type type, Node* parent) : _parent(parent)
{
    _node = NULL;

    switch (type)
    {
    case PLIST_BOOLEAN:
        _node = plist_new_bool(0);
        break;
    case PLIST_UINT:
        _node = plist_new_uint(0);
        break;
    case PLIST_REAL:
        _node = plist_new_real(0.);
        break;
    case PLIST_STRING:
        _node = plist_new_string("");
        break;
    case PLIST_KEY:
        _node = plist_new_string("");
        plist_set_key_val(_node, "");
        break;
    case PLIST_UID:
	_node = plist_new_uid(0);
	break;
    case PLIST_DATA:
        _node = plist_new_data(NULL,0);
        break;
    case PLIST_DATE:
        _node = plist_new_date(0,0);
        break;
    case PLIST_ARRAY:
        _node = plist_new_array();
        break;
    case PLIST_DICT:
        _node = plist_new_dict();
        break;
    case PLIST_NONE:
    default:
        break;
    }
}

Node::~Node()
{
	/* If the Node is in a container, let _node be cleaned up by
	 * operations on the parent plist_t. Otherwise, duplicate frees
	 * occur when a Node is removed from or replaced in a Dictionary.
	 */
	if (_parent == NULL)
		plist_free(_node);
    _node = NULL;
    _parent = NULL;
}

plist_type Node::GetType() const
{
    if (_node)
    {
        return plist_get_node_type(_node);
    }
    return PLIST_NONE;
}

plist_t Node::GetPlist() const
{
    return _node;
}

Node* Node::GetParent() const
{
    return _parent;
}

Node* Node::FromPlist(plist_t node, Node* parent)
{
    Node* ret = NULL;
    if (node)
    {
        plist_type type = plist_get_node_type(node);
        switch (type)
        {
        case PLIST_DICT:
            ret = new Dictionary(node, parent);
            break;
        case PLIST_ARRAY:
            ret = new Array(node, parent);
            break;
        case PLIST_BOOLEAN:
            ret = new Boolean(node, parent);
            break;
        case PLIST_UINT:
            ret = new Integer(node, parent);
            break;
        case PLIST_REAL:
            ret = new Real(node, parent);
            break;
        case PLIST_STRING:
            ret = new String(node, parent);
            break;
        case PLIST_KEY:
            ret = new Key(node, parent);
            break;
        case PLIST_UID:
            ret = new Uid(node, parent);
            break;
        case PLIST_DATE:
            ret = new Date(node, parent);
            break;
        case PLIST_DATA:
            ret = new Data(node, parent);
            break;
        default:
            plist_free(node);
            break;
        }
    }
    return ret;
}

}  // namespace PList
