/*
 * Array.cpp
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

#include <plist/Array.h>

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace PList
{

Array::Array(Node* parent) : Structure(PLIST_ARRAY, parent)
{
    _array.clear();
}

static void array_fill(Array *_this, std::vector<Node*> array, plist_t node)
{
    plist_array_iter iter = NULL;
    plist_array_new_iter(node, &iter);
    plist_t subnode;
    do {
        subnode = NULL;
        plist_array_next_item(node, iter, &subnode);
        array.push_back( Node::FromPlist(subnode, _this) );
    } while (subnode);
    free(iter);
}

Array::Array(plist_t node, Node* parent) : Structure(parent)
{
    _node = node;
    array_fill(this, _array, _node);
}

Array::Array(const PList::Array& a)
{
    _array.clear();
    _node = plist_copy(a.GetPlist());
    array_fill(this, _array, _node);
}

Array& Array::operator=(PList::Array& a)
{
    plist_free(_node);
    for (unsigned int it = 0; it < _array.size(); it++)
    {
        delete _array.at(it);
    }
    _array.clear();
    _node = plist_copy(a.GetPlist());
    array_fill(this, _array, _node);
    return *this;
}

Array::~Array()
{
    for (unsigned int it = 0; it < _array.size(); it++)
    {
        delete (_array.at(it));
    }
    _array.clear();
}

Node* Array::Clone() const
{
    return new Array(*this);
}

Node* Array::operator[](unsigned int array_index)
{
    return _array.at(array_index);
}

void Array::Append(Node* node)
{
    if (node)
    {
        Node* clone = node->Clone();
        UpdateNodeParent(clone);
        plist_array_append_item(_node, clone->GetPlist());
        _array.push_back(clone);
    }
}

void Array::Insert(Node* node, unsigned int pos)
{
    if (node)
    {
        Node* clone = node->Clone();
        UpdateNodeParent(clone);
        plist_array_insert_item(_node, clone->GetPlist(), pos);
        std::vector<Node*>::iterator it = _array.begin();
        it += pos;
        _array.insert(it, clone);
    }
}

void Array::Remove(Node* node)
{
    if (node)
    {
        uint32_t pos = plist_array_get_item_index(node->GetPlist());
        if (pos == UINT_MAX) {
            return;
        }
        plist_array_remove_item(_node, pos);
        std::vector<Node*>::iterator it = _array.begin();
        it += pos;
        _array.erase(it);
        delete node;
    }
}

void Array::Remove(unsigned int pos)
{
    plist_array_remove_item(_node, pos);
    std::vector<Node*>::iterator it = _array.begin();
    it += pos;
    delete _array.at(pos);
    _array.erase(it);
}

unsigned int Array::GetNodeIndex(Node* node) const
{
    std::vector<Node*>::const_iterator it = std::find(_array.begin(), _array.end(), node);
    return std::distance (_array.begin(), it);
}

}  // namespace PList
