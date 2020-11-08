/*
 * Date.h
 * Date node type for C++ binding
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

#ifndef PLIST_DATE_H
#define PLIST_DATE_H

#include <plist/Node.h>
#include <ctime>
#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

namespace PList
{

class Date : public Node
{
public :
    Date(Node* parent = NULL);
    Date(plist_t node, Node* parent = NULL);
    Date(const Date& d);
    Date& operator=(Date& d);
    Date(timeval t);
    virtual ~Date();

    Node* Clone() const;

    void SetValue(timeval t);
    timeval GetValue() const;
};

};

#endif // PLIST_DATE_H
