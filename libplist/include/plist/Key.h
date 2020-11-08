/*
 * Key.h
 * Key node type for C++ binding
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

#ifndef PLIST_KEY_H
#define PLIST_KEY_H

#include <plist/Node.h>
#include <string>

namespace PList
{

class Key : public Node
{
public :
    Key(Node* parent = NULL);
    Key(plist_t node, Node* parent = NULL);
    Key(const Key& k);
    Key& operator=(Key& k);
    Key(const std::string& s);
    virtual ~Key();

    Node* Clone() const;

    void SetValue(const std::string& s);
    std::string GetValue() const;
};

};

#endif // PLIST_KEY_H
