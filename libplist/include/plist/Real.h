/*
 * Real.h
 * Real node type for C++ binding
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

#ifndef PLIST_REAL_H
#define PLIST_REAL_H

#include <plist/Node.h>

namespace PList
{

class Real : public Node
{
public :
    Real(Node* parent = NULL);
    Real(plist_t node, Node* parent = NULL);
    Real(const Real& d);
    Real& operator=(Real& d);
    Real(double d);
    virtual ~Real();

    Node* Clone() const;

    void SetValue(double d);
    double GetValue() const;
};

};

#endif // PLIST_REAL_H
