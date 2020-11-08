#include <Python.h>

void datetime_to_ints(PyObject* obj, int32_t* sec, int32_t* usec);
PyObject* ints_to_datetime(int32_t sec, int32_t usec);
int check_datetime(PyObject* obj);
