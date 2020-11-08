/*
 * Dictionary.cpp
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
#include <plist/Dictionary.h>

namespace PList
{

Dictionary::Dictionary(Node* parent) : Structure(PLIST_DICT, parent)
{
}

static void dictionary_fill(Dictionary *_this, std::map<std::string,Node*> &map, plist_t node)
{
    plist_dict_iter it = NULL;
    plist_dict_new_iter(node, &it);
    plist_t subnode = NULL;
    do {
        char *key = NULL;
        subnode = NULL;
        plist_dict_next_item(node, it, &key, &subnode);
        if (key && subnode)
            map[std::string(key)] = Node::FromPlist(subnode, _this);
        free(key);
    } while (subnode);
    free(it);
}

Dictionary::Dictionary(plist_t node, Node* parent) : Structure(parent)
{
    _node = node;
    dictionary_fill(this, _map, _node);
}

Dictionary::Dictionary(const PList::Dictionary& d)
{
    for (Dictionary::iterator it = _map.begin(); it != _map.end(); it++)
    {
        plist_free(it->second->GetPlist());
        delete it->second;
    }
    _map.clear();
    _node = plist_copy(d.GetPlist());
    dictionary_fill(this, _map, _node);
}

Dictionary& Dictionary::operator=(PList::Dictionary& d)
{
    for (Dictionary::iterator it = _map.begin(); it != _map.end(); it++)
    {
        plist_free(it->second->GetPlist());
        delete it->second;
    }
    _map.clear();
    _node = plist_copy(d.GetPlist());
    dictionary_fill(this, _map, _node);
    return *this;
}

Dictionary::~Dictionary()
{
    for (Dictionary::iterator it = _map.begin(); it != _map.end(); it++)
    {
        delete it->second;
    }
    _map.clear();
}

Node* Dictionary::Clone() const
{
    return new Dictionary(*this);
}

Node* Dictionary::operator[](const std::string& key)
{
    return _map[key];
}

Dictionary::iterator Dictionary::Begin()
{
    return _map.begin();
}

Dictionary::iterator Dictionary::End()
{
    return _map.end();
}

Dictionary::const_iterator Dictionary::Begin() const
{
    return _map.begin();
}

Dictionary::const_iterator Dictionary::End() const
{
    return _map.end();
}

Dictionary::iterator Dictionary::Find(const std::string& key)
{
    return _map.find(key);
}

Dictionary::const_iterator Dictionary::Find(const std::string& key) const
{
    return _map.find(key);
}

Dictionary::iterator Dictionary::Set(const std::string& key, const Node* node)
{
    if (node)
    {
        Node* clone = node->Clone();
        UpdateNodeParent(clone);
        plist_dict_set_item(_node, key.c_str(), clone->GetPlist());
        delete _map[key];
        _map[key] = clone;
        return _map.find(key);
    }
    return iterator(this->_map.end());
}

Dictionary::iterator Dictionary::Set(const std::string& key, const Node& node)
{
    return Set(key, &node);
}

Dictionary::iterator Dictionary::Insert(const std::string& key, Node* node)
{
    return this->Set(key, node);
}

void Dictionary::Remove(Node* node)
{
    if (node)
    {
        char* key = NULL;
        plist_dict_get_item_key(node->GetPlist(), &key);
        plist_dict_remove_item(_node, key);
        std::string skey = key;
        free(key);
        _map.erase(skey);
        delete node;
    }
}

void Dictionary::Remove(const std::string& key)
{
    plist_dict_remove_item(_node, key.c_str());
    delete _map[key];
    _map.erase(key);
}

std::string Dictionary::GetNodeKey(Node* node)
{
    for (iterator it = _map.begin(); it != _map.end(); ++it)
    {
        if (it->second == node)
            return it->first;
    }
    return "";
}

}  // namespace PList
