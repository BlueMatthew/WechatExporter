from libc.stdint cimport *

cdef extern from "plist/plist.h":
    ctypedef void *plist_t
    ctypedef void *plist_dict_iter
    void plist_free(plist_t node)

cdef class Node:
    cdef plist_t _c_node
    cdef bint _c_managed
    cpdef object __deepcopy__(self, memo=*)
    cpdef unicode to_xml(self)
    cpdef bytes to_bin(self)
    cpdef object copy(self)

cdef class Bool(Node):
    cpdef set_value(self, object value)
    cpdef bint get_value(self)

cdef class Integer(Node):
    cpdef set_value(self, object value)
    cpdef uint64_t get_value(self)

cdef class Uid(Node):
    cpdef set_value(self, object value)
    cpdef uint64_t get_value(self)

cdef class Key(Node):
    cpdef set_value(self, object value)
    cpdef unicode get_value(self)

cdef class Real(Node):
    cpdef set_value(self, object value)
    cpdef float get_value(self)

cdef class String(Node):
    cpdef set_value(self, object value)
    cpdef unicode get_value(self)

cdef class Date(Node):
    cpdef set_value(self, object value)
    cpdef object get_value(self)

cdef class Data(Node):
    cpdef set_value(self, object value)
    cpdef bytes get_value(self)

cdef class Dict(Node):
    cdef dict _map
    cdef void _init(self)
    cpdef set_value(self, dict value)
    cpdef dict get_value(self)
    cpdef bint has_key(self, key)
    cpdef object get(self, key, default=*)
    cpdef list keys(self)
    cpdef list items(self)
    cpdef list values(self)
    cpdef object iterkeys(self)
    cpdef object iteritems(self)
    cpdef object itervalues(self)

cdef class Array(Node):
    cdef list _array
    cdef void _init(self)
    cpdef set_value(self, value)
    cpdef list get_value(self)
    cpdef append(self, object item)

cpdef object from_xml(xml)
cpdef object from_bin(bytes bin)

cpdef object load(fp, fmt=*, use_builtin_types=*, dict_type=*)
cpdef object loads(data, fmt=*, use_builtin_types=*, dict_type=*)
cpdef object dump(value, fp, fmt=*, sort_keys=*, skipkeys=*)
cpdef object dumps(value, fmt=*, sort_keys=*, skipkeys=*)

cdef object plist_t_to_node(plist_t c_plist, bint managed=*)
cdef plist_t native_to_plist_t(object native)
