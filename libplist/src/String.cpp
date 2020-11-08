/*
 * String.cpp
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
#include <plist/String.h>

namespace PList
{

String::String(Node* parent) : Node(PLIST_STRING, parent)
{
}

String::String(plist_t node, Node* parent) : Node(node, parent)
{
}

String::String(const PList::String& s) : Node(PLIST_UINT)
{
    plist_set_string_val(_node, s.GetValue().c_str());
}

String& String::operator=(PList::String& s)
{
    plist_free(_node);
    _node = plist_copy(s.GetPlist());
    return *this;
}

String::String(const std::string& s) : Node(PLIST_STRING)
{
    plist_set_string_val(_node, s.c_str());
}

String::~String()
{
}

Node* String::Clone() const
{
    return new String(*this);
}

void String::SetValue(const std::string& s)
{
    plist_set_string_val(_node, s.c_str());
}

std::string String::GetValue() const
{
    char* s = NULL;
    plist_get_string_val(_node, &s);
    std::string ret;
    if (s) {
        ret = s;
        free(s);
    } else {
        ret = "";
    }
    return ret;
}

}  // namespace PList
