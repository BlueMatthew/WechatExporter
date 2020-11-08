/*
 * Structure.h
 * Structure node type for C++ binding
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

#ifndef PLIST_STRUCTURE_H
#define PLIST_STRUCTURE_H

#include <plist/Node.h>
#include <string>
#include <vector>

namespace PList
{

class Structure : public Node
{
public :
    virtual ~Structure();

    uint32_t GetSize() const;

    std::string ToXml() const;
    std::vector<char> ToBin() const;

    virtual void Remove(Node* node) = 0;

    static Structure* FromXml(const std::string& xml);
    static Structure* FromBin(const std::vector<char>& bin);

protected:
    Structure(Node* parent = NULL);
    Structure(plist_type type, Node* parent = NULL);
    void UpdateNodeParent(Node* node);

private:
    Structure(Structure& s);
    Structure& operator=(const Structure& s);
};

};

#endif // PLIST_STRUCTURE_H
