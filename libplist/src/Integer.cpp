/*
 * Integer.cpp
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
#include <plist/Integer.h>

namespace PList
{

Integer::Integer(Node* parent) : Node(PLIST_UINT, parent)
{
}

Integer::Integer(plist_t node, Node* parent) : Node(node, parent)
{
}

Integer::Integer(const PList::Integer& i) : Node(PLIST_UINT)
{
    plist_set_uint_val(_node, i.GetValue());
}

Integer& Integer::operator=(PList::Integer& i)
{
    plist_free(_node);
    _node = plist_copy(i.GetPlist());
    return *this;
}

Integer::Integer(uint64_t i) : Node(PLIST_UINT)
{
    plist_set_uint_val(_node, i);
}

Integer::~Integer()
{
}

Node* Integer::Clone() const
{
    return new Integer(*this);
}

void Integer::SetValue(uint64_t i)
{
    plist_set_uint_val(_node, i);
}

uint64_t Integer::GetValue() const
{
    uint64_t i = 0;
    plist_get_uint_val(_node, &i);
    return i;
}

}  // namespace PList
