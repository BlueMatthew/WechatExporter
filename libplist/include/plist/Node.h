/*
 * Node.h
 * Abstract node type for C++ binding
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

#ifndef PLIST_NODE_H
#define PLIST_NODE_H

#include <plist/plist.h>
#include <cstddef>

namespace PList
{

class Node
{
public :
    virtual ~Node();

    virtual Node* Clone() const = 0;

    Node * GetParent() const;
    plist_type GetType() const;
    plist_t GetPlist() const;

    static Node* FromPlist(plist_t node, Node* parent = NULL);

protected:
    Node(Node* parent = NULL);
    Node(plist_t node, Node* parent = NULL);
    Node(plist_type type, Node* parent = NULL);
    plist_t _node;

private:
    Node* _parent;
    friend class Structure;
};

};

#endif // PLIST_NODE_H
