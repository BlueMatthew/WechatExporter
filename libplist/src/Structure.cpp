/*
 * Structure.cpp
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
#include <plist/Structure.h>

namespace PList
{

Structure::Structure(Node* parent) : Node(parent)
{
}
Structure::Structure(plist_type type, Node* parent) : Node(type, parent)
{
}

Structure::~Structure()
{
}

uint32_t Structure::GetSize() const
{
    uint32_t size = 0;
    plist_type type = plist_get_node_type(_node);
    if (type == PLIST_ARRAY)
    {
        size = plist_array_get_size(_node);
    }
    else if (type == PLIST_DICT)
    {
        size = plist_dict_get_size(_node);
    }
    return size;
}

std::string Structure::ToXml() const
{
    char* xml = NULL;
    uint32_t length = 0;
    plist_to_xml(_node, &xml, &length);
    std::string ret(xml, xml+length);
    free(xml);
    return ret;
}

std::vector<char> Structure::ToBin() const
{
    char* bin = NULL;
    uint32_t length = 0;
    plist_to_bin(_node, &bin, &length);
    std::vector<char> ret(bin, bin+length);
    free(bin);
    return ret;
}

void Structure::UpdateNodeParent(Node* node)
{
    //Unlink node first
    if ( NULL != node->_parent )
    {
        plist_type type = plist_get_node_type(node->_parent);
        if (PLIST_ARRAY ==type || PLIST_DICT == type )
        {
            Structure* s = static_cast<Structure*>(node->_parent);
            s->Remove(node);
        }
    }
    node->_parent = this;
}

static Structure* ImportStruct(plist_t root)
{
    Structure* ret = NULL;
    plist_type type = plist_get_node_type(root);

    if (PLIST_ARRAY == type || PLIST_DICT == type)
    {
        ret = static_cast<Structure*>(Node::FromPlist(root));
    }
    else
    {
        plist_free(root);
    }

    return ret;
}

Structure* Structure::FromXml(const std::string& xml)
{
    plist_t root = NULL;
    plist_from_xml(xml.c_str(), xml.size(), &root);

    return ImportStruct(root);
}

Structure* Structure::FromBin(const std::vector<char>& bin)
{
    plist_t root = NULL;
    plist_from_bin(&bin[0], bin.size(), &root);

    return ImportStruct(root);

}

}  // namespace PList

