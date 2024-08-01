#ifndef CONVERSIONS_1_H
#define CONVERSIONS_1_H

// Includes
#include <Python.h>
#include <datetime.h>
#include <ctype.h>

// The init function
void sbs1_init(void);

// The to-conversion function
PyObject *to_value_prot1(PyObject *py_bytes);

#endif // CONVERSIONS_1_H