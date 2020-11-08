/*
 * Dictionary.h
 * Dictionary node type for C++ binding
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

#ifndef PLIST_DICTIONARY_H
#define PLIST_DICTIONARY_H

#include <plist/Structure.h>
#include <map>
#include <string>

namespace PList
{

class Dictionary : public Structure
{
public :
    Dictionary(Node* parent = NULL);
    Dictionary(plist_t node, Node* parent = NULL);
    Dictionary(const Dictionary& d);
    Dictionary& operator=(Dictionary& d);
    virtual ~Dictionary();

    Node* Clone() const;

    typedef std::map<std::string,Node*>::iterator iterator;
    typedef std::map<std::string,Node*>::const_iterator const_iterator;

    Node* operator[](const std::string& key);
    iterator Begin();
    iterator End();
    iterator Find(const std::string& key);
    const_iterator Begin() const;
    const_iterator End() const;
    const_iterator Find(const std::string& key) const;
    iterator Set(const std::string& key, const Node* node);
    iterator Set(const std::string& key, const Node& node);
    PLIST_WARN_DEPRECATED("use Set() instead") iterator Insert(const std::string& key, Node* node);
    void Remove(Node* node);
    void Remove(const std::string& key);
    std::string GetNodeKey(Node* node);

private :
    std::map<std::string,Node*> _map;


};

};

#endif // PLIST_DICTIONARY_H
