#ifndef PYBYTES_H
#define PYBYTES_H

#include <Python.h>

PyObject *from_single_value(PyObject *self, PyObject *args);
PyObject *to_single_value(PyObject *self, PyObject *args);

PyObject *from_value(PyObject *self, PyObject *args);
PyObject *to_value(PyObject *self, PyObject *args);

#endif // PYBYTES_H