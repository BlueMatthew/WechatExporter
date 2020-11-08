/*
 * Boolean.h
 * Boolean node type for C++ binding
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

#ifndef PLIST_BOOLEAN_H
#define PLIST_BOOLEAN_H

#include <plist/Node.h>

namespace PList
{

class Boolean : public Node
{
public :
    Boolean(Node* parent = NULL);
    Boolean(plist_t node, Node* parent = NULL);
    Boolean(const Boolean& b);
    Boolean& operator=(Boolean& b);
    Boolean(bool b);
    virtual ~Boolean();

    Node* Clone() const;

    void SetValue(bool b);
    bool GetValue() const;
};

};

#endif // PLIST_BOOLEAN_H
