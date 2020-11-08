cimport cpython
cimport libc.stdlib
from libc.stdint cimport *

cdef extern from *:
    ctypedef enum plist_type:
        PLIST_BOOLEAN,
        PLIST_UINT,
        PLIST_REAL,
        PLIST_STRING,
        PLIST_ARRAY,
        PLIST_DICT,
        PLIST_DATE,
        PLIST_DATA,
        PLIST_KEY,
        PLIST_UID,
        PLIST_NONE

    plist_t plist_new_bool(uint8_t val)
    void plist_get_bool_val(plist_t node, uint8_t *val)
    void plist_set_bool_val(plist_t node, uint8_t val)

    plist_t plist_new_uint(uint64_t val)
    void plist_get_uint_val(plist_t node, uint64_t *val)
    void plist_set_uint_val(plist_t node, uint64_t val)

    plist_t plist_new_real(double val)
    void plist_get_real_val(plist_t node, double *val)
    void plist_set_real_val(plist_t node, double val)

    plist_t plist_new_date(int32_t sec, int32_t usec)
    void plist_get_date_val(plist_t node, int32_t * sec, int32_t * usec)
    void plist_set_date_val(plist_t node, int32_t sec, int32_t usec)

    void plist_get_key_val(plist_t node, char **val)
    void plist_set_key_val(plist_t node, char *val)

    plist_t plist_new_uid(uint64_t val)
    void plist_get_uid_val(plist_t node, uint64_t *val)
    void plist_set_uid_val(plist_t node, uint64_t val)

    plist_t plist_new_string(char *val)
    void plist_get_string_val(plist_t node, char **val)
    void plist_set_string_val(plist_t node, char *val)

    plist_t plist_new_data(char *val, uint64_t length)
    void plist_get_data_val(plist_t node, char **val, uint64_t * length)
    void plist_set_data_val(plist_t node, char *val, uint64_t length)

    plist_t plist_new_dict()
    int plist_dict_get_size(plist_t node)
    plist_t plist_dict_get_item(plist_t node, char* key)
    void plist_dict_set_item(plist_t node, char* key, plist_t item)
    void plist_dict_insert_item(plist_t node, char* key, plist_t item)
    void plist_dict_remove_item(plist_t node, char* key)

    void plist_dict_new_iter(plist_t node, plist_dict_iter *iter)
    void plist_dict_next_item(plist_t node, plist_dict_iter iter, char **key, plist_t *val)

    plist_t plist_new_array()
    uint32_t plist_array_get_size(plist_t node)
    plist_t plist_array_get_item(plist_t node, uint32_t n)
    uint32_t plist_array_get_item_index(plist_t node)
    void plist_array_set_item(plist_t node, plist_t item, uint32_t n)
    void plist_array_append_item(plist_t node, plist_t item)
    void plist_array_insert_item(plist_t node, plist_t item, uint32_t n)
    void plist_array_remove_item(plist_t node, uint32_t n)

    void plist_free(plist_t plist)
    plist_t plist_copy(plist_t plist)
    void plist_to_xml(plist_t plist, char **plist_xml, uint32_t *length)
    void plist_to_bin(plist_t plist, char **plist_bin, uint32_t *length)

    plist_t plist_get_parent(plist_t node)
    plist_type plist_get_node_type(plist_t node)

    void plist_from_xml(char *plist_xml, uint32_t length, plist_t * plist)
    void plist_from_bin(char *plist_bin, uint32_t length, plist_t * plist)

cdef class Node:
    def __init__(self, *args, **kwargs):
        self._c_managed = True

    def __dealloc__(self):
        if self._c_node is not NULL and self._c_managed:
            plist_free(self._c_node)

    cpdef object __deepcopy__(self, memo={}):
        return plist_t_to_node(plist_copy(self._c_node))

    cpdef object copy(self):
        cdef plist_t c_node = NULL
        c_node = plist_copy(self._c_node)
        return plist_t_to_node(c_node)

    cpdef unicode to_xml(self):
        cdef:
            char* out = NULL
            uint32_t length
        plist_to_xml(self._c_node, &out, &length)

        try:
            return cpython.PyUnicode_DecodeUTF8(out, length, 'strict')
        finally:
            if out != NULL:
                libc.stdlib.free(out)

    cpdef bytes to_bin(self):
        cdef:
            char* out = NULL
            uint32_t length
        plist_to_bin(self._c_node, &out, &length)

        try:
            return bytes(out[:length])
        finally:
            if out != NULL:
                libc.stdlib.free(out)

    property parent:
        def __get__(self):
            cdef plist_t c_parent = NULL
            cdef Node node

            c_parent = plist_get_parent(self._c_node)
            if c_parent == NULL:
                return None

            return plist_t_to_node(c_parent)

    def __str__(self):
        return str(self.get_value())

cdef class Bool(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        if value is None:
            self._c_node = plist_new_bool(0)
        else:
            self._c_node = plist_new_bool(bool(value))

    def __nonzero__(self):
        return self.get_value()

    def __richcmp__(self, other, op):
        cdef bint b = self.get_value()
        if op == 0:
            return b < other
        if op == 1:
            return b <= other
        if op == 2:
            return b == other
        if op == 3:
            return b != other
        if op == 4:
            return b > other
        if op == 5:
            return b >= other

    def __repr__(self):
        b = self.get_value()
        return '<Bool: %s>' % b

    cpdef set_value(self, object value):
        plist_set_bool_val(self._c_node, bool(value))

    cpdef bint get_value(self):
        cdef uint8_t value
        plist_get_bool_val(self._c_node, &value)
        return bool(value)

cdef Bool Bool_factory(plist_t c_node, bint managed=True):
    cdef Bool instance = Bool.__new__(Bool)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef class Integer(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        if value is None:
            self._c_node = plist_new_uint(0)
        else:
            self._c_node = plist_new_uint(int(value))

    def __repr__(self):
        cdef uint64_t i = self.get_value()
        return '<Integer: %s>' % i

    def __int__(self):
        return self.get_value()

    def __float__(self):
        return float(self.get_value())

    def __richcmp__(self, other, op):
        cdef int i = self.get_value()
        if op == 0:
            return i < other
        if op == 1:
            return i <= other
        if op == 2:
            return i == other
        if op == 3:
            return i != other
        if op == 4:
            return i > other
        if op == 5:
            return i >= other

    cpdef set_value(self, object value):
        plist_set_uint_val(self._c_node, int(value))

    cpdef uint64_t get_value(self):
        cdef uint64_t value
        plist_get_uint_val(self._c_node, &value)
        return value

cdef Integer Integer_factory(plist_t c_node, bint managed=True):
    cdef Integer instance = Integer.__new__(Integer)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef class Real(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        if value is None:
            self._c_node = plist_new_real(0.0)
        else:
            self._c_node = plist_new_real(float(value))

    def __repr__(self):
        r = self.get_value()
        return '<Real: %s>' % r

    def __float__(self):
        return self.get_value()

    def __int__(self):
        return int(self.get_value())

    def __richcmp__(self, other, op):
        cdef float f = self.get_value()
        if op == 0:
            return f < other
        if op == 1:
            return f <= other
        if op == 2:
            return f == other
        if op == 3:
            return f != other
        if op == 4:
            return f > other
        if op == 5:
            return f >= other

    cpdef set_value(self, object value):
        plist_set_real_val(self._c_node, float(value))

    cpdef float get_value(self):
        cdef double value
        plist_get_real_val(self._c_node, &value)
        return value

cdef Real Real_factory(plist_t c_node, bint managed=True):
    cdef Real instance = Real.__new__(Real)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef class Uid(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        if value is None:
            self._c_node = plist_new_uid(0)
        else:
            self._c_node = plist_new_uid(int(value))

    def __repr__(self):
        cdef uint64_t i = self.get_value()
        return '<Uid: %s>' % i

    def __int__(self):
        return self.get_value()

    def __float__(self):
        return float(self.get_value())

    def __richcmp__(self, other, op):
        cdef int i = self.get_value()
        if op == 0:
            return i < other
        if op == 1:
            return i <= other
        if op == 2:
            return i == other
        if op == 3:
            return i != other
        if op == 4:
            return i > other
        if op == 5:
            return i >= other

    cpdef set_value(self, object value):
        plist_set_uid_val(self._c_node, int(value))

    cpdef uint64_t get_value(self):
        cdef uint64_t value
        plist_get_uid_val(self._c_node, &value)
        return value

cdef Uid Uid_factory(plist_t c_node, bint managed=True):
    cdef Uid instance = Uid.__new__(Uid)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

from cpython cimport PY_MAJOR_VERSION

cdef class Key(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        cdef:
            char* c_utf8_data = NULL
            bytes utf8_data
        if value is None:
            raise ValueError("Requires a value")
        else:
            if isinstance(value, unicode):
                utf8_data = value.encode('utf-8')
            elif (PY_MAJOR_VERSION < 3) and isinstance(value, str):
                value.encode('ascii') # trial decode
                utf8_data = value.encode('ascii')
            else:
                raise ValueError("Requires unicode input, got %s" % type(value))
            c_utf8_data = utf8_data
            self._c_node = plist_new_string("")
            plist_set_key_val(self._c_node, c_utf8_data)

    def __repr__(self):
        s = self.get_value()
        return '<Key: %s>' % s.encode('utf-8')

    def __richcmp__(self, other, op):
        cdef unicode s = self.get_value()
        if op == 0:
            return s < other
        if op == 1:
            return s <= other
        if op == 2:
            return s == other
        if op == 3:
            return s != other
        if op == 4:
            return s > other
        if op == 5:
            return s >= other

    cpdef set_value(self, object value):
        cdef:
            char* c_utf8_data = NULL
            bytes utf8_data
        if value is None:
            plist_set_key_val(self._c_node, c_utf8_data)
        else:
            if isinstance(value, unicode):
                utf8_data = value.encode('utf-8')
            elif (PY_MAJOR_VERSION < 3) and isinstance(value, str):
                value.encode('ascii') # trial decode
                utf8_data = value.encode('ascii')
            else:
                raise ValueError("Requires unicode input, got %s" % type(value))
            c_utf8_data = utf8_data
            plist_set_key_val(self._c_node, c_utf8_data)

    cpdef unicode get_value(self):
        cdef:
            char* c_value = NULL
        plist_get_key_val(self._c_node, &c_value)
        try:
            return cpython.PyUnicode_DecodeUTF8(c_value, len(c_value), 'strict')
        finally:
            libc.stdlib.free(c_value)

cdef Key Key_factory(plist_t c_node, bint managed=True):
    cdef Key instance = Key.__new__(Key)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef class String(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        cdef:
            char* c_utf8_data = NULL
            bytes utf8_data
        if value is None:
            self._c_node = plist_new_string("")
        else:
            if isinstance(value, unicode):
                utf8_data = value.encode('utf-8')
            elif (PY_MAJOR_VERSION < 3) and isinstance(value, str):
                value.encode('ascii') # trial decode
                utf8_data = value.encode('ascii')
            else:
                raise ValueError("Requires unicode input, got %s" % type(value))
            c_utf8_data = utf8_data
            self._c_node = plist_new_string(c_utf8_data)

    def __repr__(self):
        s = self.get_value()
        return '<String: %s>' % s.encode('utf-8')

    def __richcmp__(self, other, op):
        cdef unicode s = self.get_value()
        if op == 0:
            return s < other
        if op == 1:
            return s <= other
        if op == 2:
            return s == other
        if op == 3:
            return s != other
        if op == 4:
            return s > other
        if op == 5:
            return s >= other

    cpdef set_value(self, object value):
        cdef:
            char* c_utf8_data = NULL
            bytes utf8_data
        if value is None:
            plist_set_string_val(self._c_node, c_utf8_data)
        else:
            if isinstance(value, unicode):
                utf8_data = value.encode('utf-8')
            elif (PY_MAJOR_VERSION < 3) and isinstance(value, str):
                value.encode('ascii') # trial decode
                utf8_data = value.encode('ascii')
            else:
                raise ValueError("Requires unicode input, got %s" % type(value))
            c_utf8_data = utf8_data
            plist_set_string_val(self._c_node, c_utf8_data)

    cpdef unicode get_value(self):
        cdef:
            char* c_value = NULL
        plist_get_string_val(self._c_node, &c_value)
        try:
            return cpython.PyUnicode_DecodeUTF8(c_value, len(c_value), 'strict')
        finally:
            libc.stdlib.free(c_value)

cdef String String_factory(plist_t c_node, bint managed=True):
    cdef String instance = String.__new__(String)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

MAC_EPOCH = 978307200

cdef extern from "plist_util.h":
    void datetime_to_ints(object obj, int32_t* sec, int32_t* usec)
    object ints_to_datetime(int32_t sec, int32_t usec)
    int check_datetime(object obj)

cdef plist_t create_date_plist(value=None):
    cdef plist_t node = NULL
    cdef int32_t secs
    cdef int32_t usecs
    if value is None:
        node = plist_new_date(0, 0)
    elif check_datetime(value):
        datetime_to_ints(value, &secs, &usecs)
        secs -= MAC_EPOCH
        node = plist_new_date(secs, usecs)
    return node

cdef class Date(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        self._c_node = create_date_plist(value)

    def __repr__(self):
        d = self.get_value()
        return '<Date: %s>' % d.ctime()

    def __richcmp__(self, other, op):
        d = self.get_value()
        if op == 0:
            return d < other
        if op == 1:
            return d <= other
        if op == 2:
            return d == other
        if op == 3:
            return d != other
        if op == 4:
            return d > other
        if op == 5:
            return d >= other

    cpdef object get_value(self):
        cdef int32_t secs = 0
        cdef int32_t usecs = 0
        plist_get_date_val(self._c_node, &secs, &usecs)
        secs += MAC_EPOCH
        return ints_to_datetime(secs, usecs)

    cpdef set_value(self, object value):
        cdef int32_t secs
        cdef int32_t usecs
        if not check_datetime(value):
            raise ValueError("Expected a datetime")
        datetime_to_ints(value, &secs, &usecs)
        plist_set_date_val(self._c_node, secs, usecs)

cdef Date Date_factory(plist_t c_node, bint managed=True):
    cdef Date instance = Date.__new__(Date)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef class Data(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        if value is None:
            self._c_node = plist_new_data(NULL, 0)
        else:
            self._c_node = plist_new_data(value, len(value))

    def __repr__(self):
        d = self.get_value()
        return '<Data: %s>' % d

    def __richcmp__(self, other, op):
        cdef bytes d = self.get_value()
        if op == 0:
            return d < other
        if op == 1:
            return d <= other
        if op == 2:
            return d == other
        if op == 3:
            return d != other
        if op == 4:
            return d > other
        if op == 5:
            return d >= other

    cpdef bytes get_value(self):
        cdef:
            char* val = NULL
            uint64_t length = 0
        plist_get_data_val(self._c_node, &val, &length)

        try:
            return bytes(val[:length])
        finally:
            libc.stdlib.free(val)

    cpdef set_value(self, object value):
        cdef:
            bytes py_val = value
        plist_set_data_val(self._c_node, py_val, len(value))

cdef Data Data_factory(plist_t c_node, bint managed=True):
    cdef Data instance = Data.__new__(Data)
    instance._c_managed = managed
    instance._c_node = c_node
    return instance

cdef plist_t create_dict_plist(object value=None):
    cdef plist_t node = NULL
    cdef plist_t c_node = NULL
    node = plist_new_dict()
    if value is not None and isinstance(value, dict):
        for key, item in value.items():
            c_node = native_to_plist_t(item)
            plist_dict_set_item(node, key, c_node)
            c_node = NULL
    return node

cimport cpython

cdef class Dict(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        self._c_node = create_dict_plist(value)

    def __init__(self, value=None, *args, **kwargs):
        self._init()

    cdef void _init(self):
        cdef plist_dict_iter it = NULL
        cdef char* key = NULL
        cdef plist_t subnode = NULL

        self._map = cpython.PyDict_New()

        plist_dict_new_iter(self._c_node, &it);
        plist_dict_next_item(self._c_node, it, &key, &subnode);

        while subnode is not NULL:
            py_key = key

            if PY_MAJOR_VERSION >= 3:
                py_key = py_key.decode('utf-8')

            cpython.PyDict_SetItem(self._map, py_key, plist_t_to_node(subnode, False))
            subnode = NULL
            libc.stdlib.free(key)
            key = NULL
            plist_dict_next_item(self._c_node, it, &key, &subnode);
        libc.stdlib.free(it)

    def __dealloc__(self):
        self._map = None

    def __richcmp__(self, other, op):
        cdef dict d = self.get_value()
        if op == 0:
            return d < other
        if op == 1:
            return d <= other
        if op == 2:
            return d == other
        if op == 3:
            return d != other
        if op == 4:
            return d > other
        if op == 5:
            return d >= other

    def __len__(self):
        return cpython.PyDict_Size(self._map)

    def __repr__(self):
        return '<Dict: %s>' % self._map

    cpdef dict get_value(self):
        return dict([(key, value.get_value()) for key, value in self.items()])

    cpdef set_value(self, dict value):
        plist_free(self._c_node)
        self._map = {}
        self._c_node = NULL
        self._c_node = create_dict_plist(value)
        self._init()

    def __iter__(self):
        return self._map.__iter__()

    cpdef bint has_key(self, key):
        return self._map.has_key(key)

    cpdef object get(self, key, default=None):
        return self._map.get(key, default)

    cpdef list keys(self):
        return cpython.PyDict_Keys(self._map)

    cpdef object iterkeys(self):
        return self._map.iterkeys()

    cpdef list items(self):
        return cpython.PyDict_Items(self._map)

    cpdef object iteritems(self):
        return self._map.iteritems()

    cpdef list values(self):
        return cpython.PyDict_Values(self._map)

    cpdef object itervalues(self):
        return self._map.itervalues()

    def __getitem__(self, key):
        return self._map[key]

    def __setitem__(self, key, value):
        cdef Node n
        if isinstance(value, Node):
            n = value.copy()
        else:
            n = plist_t_to_node(native_to_plist_t(value), False)

        plist_dict_set_item(self._c_node, key, n._c_node)
        self._map[key] = n

    def __delitem__(self, key):
        cpython.PyDict_DelItem(self._map, key)
        plist_dict_remove_item(self._c_node, key)

cdef Dict Dict_factory(plist_t c_node, bint managed=True):
    cdef Dict instance = Dict.__new__(Dict)
    instance._c_managed = managed
    instance._c_node = c_node
    instance._init()
    return instance

cdef plist_t create_array_plist(object value=None):
    cdef plist_t node = NULL
    cdef plist_t c_node = NULL
    node = plist_new_array()
    if value is not None and (isinstance(value, list) or isinstance(value, tuple)):
        for item in value:
            c_node = native_to_plist_t(item)
            plist_array_append_item(node, c_node)
            c_node = NULL
    return node

cdef class Array(Node):
    def __cinit__(self, object value=None, *args, **kwargs):
        self._c_node = create_array_plist(value)

    def __init__(self, value=None, *args, **kwargs):
        self._init()

    cdef void _init(self):
        self._array = []
        cdef uint32_t size = plist_array_get_size(self._c_node)
        cdef plist_t subnode = NULL

        for i in range(size):
            subnode = plist_array_get_item(self._c_node, i)
            self._array.append(plist_t_to_node(subnode, False))

    def __richcmp__(self, other, op):
        cdef list l = self.get_value()
        if op == 0:
            return l < other
        if op == 1:
            return l <= other
        if op == 2:
            return l == other
        if op == 3:
            return l != other
        if op == 4:
            return l > other
        if op == 5:
            return l >= other

    def __len__(self):
        return len(self._array)

    def __repr__(self):
        return '<Array: %s>' % self._array

    cpdef list get_value(self):
        return [i.get_value() for i in self]

    cpdef set_value(self, object value):
        self._array = []
        plist_free(self._c_node)
        self._c_node = NULL
        self._c_node = create_array_plist(value)
        self._init()

    def __iter__(self):
        return self._array.__iter__()

    def __getitem__(self, index):
        return self._array[index]

    def __setitem__(self, index, value):
        cdef Node n
        if isinstance(value, Node):
            n = value.copy()
        else:
            n = plist_t_to_node(native_to_plist_t(value), False)

        if index < 0:
            index = len(self) + index

        plist_array_set_item(self._c_node, n._c_node, index)
        self._array[index] = n

    def __delitem__(self, index):
        if index < 0:
            index = len(self) + index
        del self._array[index]
        plist_array_remove_item(self._c_node, index)

    cpdef append(self, object item):
        cdef Node n

        if isinstance(item, Node):
            n = item.copy()
        else:
            n = plist_t_to_node(native_to_plist_t(item), False)

        plist_array_append_item(self._c_node, n._c_node)
        self._array.append(n)

cdef Array Array_factory(plist_t c_node, bint managed=True):
    cdef Array instance = Array.__new__(Array)
    instance._c_managed = managed
    instance._c_node = c_node
    instance._init()
    return instance

cpdef object from_xml(xml):
    cdef plist_t c_node = NULL
    plist_from_xml(xml, len(xml), &c_node)
    return plist_t_to_node(c_node)

cpdef object from_bin(bytes bin):
    cdef plist_t c_node = NULL
    plist_from_bin(bin, len(bin), &c_node)
    return plist_t_to_node(c_node)

cdef plist_t native_to_plist_t(object native):
    cdef plist_t c_node
    cdef plist_t child_c_node
    cdef int32_t secs = 0
    cdef int32_t usecs = 0
    cdef Node node
    if isinstance(native, Node):
        node = native
        return plist_copy(node._c_node)
    if isinstance(native, basestring):
        return plist_new_string(native)
    if isinstance(native, bool):
        return plist_new_bool(<bint>native)
    if isinstance(native, int) or isinstance(native, long):
        return plist_new_uint(native)
    if isinstance(native, float):
        return plist_new_real(native)
    if isinstance(native, dict):
        return create_dict_plist(native)
    if isinstance(native, list) or isinstance(native, tuple):
        return create_array_plist(native)
    if check_datetime(native):
        return create_date_plist(native)

cdef object plist_t_to_node(plist_t c_plist, bint managed=True):
    cdef plist_type t = plist_get_node_type(c_plist)
    if t == PLIST_BOOLEAN:
        return Bool_factory(c_plist, managed)
    if t == PLIST_UINT:
        return Integer_factory(c_plist, managed)
    if t == PLIST_KEY:
        return Key_factory(c_plist, managed)
    if t == PLIST_REAL:
        return Real_factory(c_plist, managed)
    if t == PLIST_STRING:
        return String_factory(c_plist, managed)
    if t == PLIST_ARRAY:
        return Array_factory(c_plist, managed)
    if t == PLIST_DICT:
        return Dict_factory(c_plist, managed)
    if t == PLIST_DATE:
        return Date_factory(c_plist, managed)
    if t == PLIST_DATA:
        return Data_factory(c_plist, managed)
    if t == PLIST_UID:
        return Uid_factory(c_plist, managed)
    if t == PLIST_NONE:
        return None

# This is to match up with the new plistlib API
# http://docs.python.org/dev/library/plistlib.html
# dump() and dumps() are not yet implemented
FMT_XML = 1
FMT_BINARY = 2

cpdef object load(fp, fmt=None, use_builtin_types=True, dict_type=dict):
    is_binary = fp.read(6) == 'bplist'
    fp.seek(0)

    cdef object cb = None

    if not fmt:
        if is_binary:
            if 'b' not in fp.mode:
                raise IOError('File handle must be opened in binary (b) mode to read binary property lists')
            cb = from_bin
        else:
            cb = from_xml
    else:
        if fmt not in (FMT_XML, FMT_BINARY):
            raise ValueError('Format must be constant FMT_XML or FMT_BINARY')
        if fmt == FMT_BINARY:
            cb = from_bin
        elif fmt == FMT_XML:
            cb = from_xml

    if is_binary and fmt == FMT_XML:
        raise ValueError('Cannot parse binary property list as XML')
    elif not is_binary and fmt == FMT_BINARY:
        raise ValueError('Cannot parse XML property list as binary')

    return cb(fp.read())

cpdef object loads(data, fmt=None, use_builtin_types=True, dict_type=dict):
    is_binary = data[0:6] == 'bplist'

    cdef object cb = None

    if fmt is not None:
        if fmt not in (FMT_XML, FMT_BINARY):
            raise ValueError('Format must be constant FMT_XML or FMT_BINARY')
        if fmt == FMT_BINARY:
            cb = from_bin
        else:
            cb = from_xml
    else:
        if is_binary:
            cb = from_bin
        else:
            cb = from_xml

    if is_binary and fmt == FMT_XML:
        raise ValueError('Cannot parse binary property list as XML')
    elif not is_binary and fmt == FMT_BINARY:
        raise ValueError('Cannot parse XML property list as binary')

    return cb(data)

cpdef object dump(value, fp, fmt=FMT_XML, sort_keys=True, skipkeys=False):
    fp.write(dumps(value, fmt=fmt))

cpdef object dumps(value, fmt=FMT_XML, sort_keys=True, skipkeys=False):
    if fmt not in (FMT_XML, FMT_BINARY):
        raise ValueError('Format must be constant FMT_XML or FMT_BINARY')

    if check_datetime(value):
        node = Date(value)
    elif isinstance(value, unicode):
        node = String(value)
    elif PY_MAJOR_VERSION >= 3 and isinstance(value, bytes):
        node = Data(value)
    elif isinstance(value, str):
        # See if this is binary
        try:
            node = String(value)
        except ValueError:
            node = Data(value)
    elif isinstance(value, bool):
        node = Bool(value)
    elif isinstance(value, int):
        node = Integer(value)
    elif isinstance(value, float):
        node = Real(value)
    elif isinstance(value, dict):
        node = Dict(value)
    elif type(value) in (list, set, tuple):
        node = Array(value)

    if fmt == FMT_XML:
        return node.to_xml()

    return node.to_bin()
