/*
 * Uid.cpp
 *
 * Copyright (c) 2012 Nikias Bassen, All Rights Reserved.
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
#include <plist/Uid.h>

namespace PList
{

Uid::Uid(Node* parent) : Node(PLIST_UID, parent)
{
}

Uid::Uid(plist_t node, Node* parent) : Node(node, parent)
{
}

Uid::Uid(const PList::Uid& i) : Node(PLIST_UID)
{
    plist_set_uid_val(_node, i.GetValue());
}

Uid& Uid::operator=(PList::Uid& i)
{
    plist_free(_node);
    _node = plist_copy(i.GetPlist());
    return *this;
}

Uid::Uid(uint64_t i) : Node(PLIST_UID)
{
    plist_set_uid_val(_node, i);
}

Uid::~Uid()
{
}

Node* Uid::Clone() const
{
    return new Uid(*this);
}

void Uid::SetValue(uint64_t i)
{
    plist_set_uid_val(_node, i);
}

uint64_t Uid::GetValue() const
{
    uint64_t i = 0;
    plist_get_uid_val(_node, &i);
    return i;
}

}  // namespace PList
