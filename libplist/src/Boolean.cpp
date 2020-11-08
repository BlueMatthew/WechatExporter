/*
 * Boolean.cpp
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
#include <plist/Boolean.h>

namespace PList
{

Boolean::Boolean(Node* parent) : Node(PLIST_BOOLEAN, parent)
{
}

Boolean::Boolean(plist_t node, Node* parent) : Node(node, parent)
{
}

Boolean::Boolean(const PList::Boolean& b) : Node(PLIST_BOOLEAN)
{
    plist_set_bool_val(_node, b.GetValue());
}

Boolean& Boolean::operator=(PList::Boolean& b)
{
    plist_free(_node);
    _node = plist_copy(b.GetPlist());
    return *this;
}

Boolean::Boolean(bool b) : Node(PLIST_BOOLEAN)
{
    plist_set_bool_val(_node, b);
}

Boolean::~Boolean()
{
}

Node* Boolean::Clone() const
{
    return new Boolean(*this);
}

void Boolean::SetValue(bool b)
{
    plist_set_bool_val(_node, b);
}

bool Boolean::GetValue() const
{
    uint8_t b = 0;
    plist_get_bool_val(_node, &b);
    return b != 0 ;
}

}  // namespace PList
