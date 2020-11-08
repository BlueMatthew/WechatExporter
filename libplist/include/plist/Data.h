/*
 * Data.h
 * Data node type for C++ binding
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

#ifndef PLIST_DATA_H
#define PLIST_DATA_H

#include <plist/Node.h>
#include <vector>

namespace PList
{

class Data : public Node
{
public :
    Data(Node* parent = NULL);
    Data(plist_t node, Node* parent = NULL);
    Data(const Data& d);
    Data& operator=(Data& b);
    Data(const std::vector<char>& buff);
    virtual ~Data();

    Node* Clone() const;

    void SetValue(const std::vector<char>& buff);
    std::vector<char> GetValue() const;
};

};

#endif // PLIST_DATA_H
