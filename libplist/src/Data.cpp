/*
 * Data.cpp
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
#include <plist/Data.h>

namespace PList
{

Data::Data(Node* parent) : Node(PLIST_DATA, parent)
{
}

Data::Data(plist_t node, Node* parent) : Node(node, parent)
{
}

Data::Data(const PList::Data& d) : Node(PLIST_DATA)
{
    std::vector<char> b = d.GetValue();
    plist_set_data_val(_node, &b[0], b.size());
}

Data& Data::operator=(PList::Data& b)
{
    plist_free(_node);
    _node = plist_copy(b.GetPlist());
    return *this;
}

Data::Data(const std::vector<char>& buff) : Node(PLIST_DATA)
{
    plist_set_data_val(_node, &buff[0], buff.size());
}

Data::~Data()
{
}

Node* Data::Clone() const
{
    return new Data(*this);
}

void Data::SetValue(const std::vector<char>& buff)
{
    plist_set_data_val(_node, &buff[0], buff.size());
}

std::vector<char> Data::GetValue() const
{
    char* buff = NULL;
    uint64_t length = 0;
    plist_get_data_val(_node, &buff, &length);
    std::vector<char> ret(buff, buff + length);
    free(buff);
    return ret;
}



}  // namespace PList
