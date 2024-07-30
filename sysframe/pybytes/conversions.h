#ifndef CONVERSIONS_H
#define CONVERSIONS_H

// Python define statement
#define PY_SSIZE_T_CLEAN

// Includes
#include <Python.h>
#include <datetime.h>
#include <ctype.h>

// Initialize the conversions module
int conversions_init(void);
// Cleanup the conversions module
void conversions_cleanup(void);

// Convert a value to bytes
PyObject *from_value(PyObject *value);
// Convert a bytes object to the value it used to be
PyObject *to_value(PyObject *bytes);

#endif // CONVERSIONS_H