#ifndef CONVERSIONS_H
#define CONVERSIONS_H

// Define statement for Python
#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <datetime.h>
#include <ctype.h>

// Datetime module classes
extern PyObject *datetime_dt; // datetime
extern PyObject *datetime_td; // timedelta
extern PyObject *datetime_d;  // date
extern PyObject *datetime_t;  // time

// UUID module class
extern PyObject *uuid_cl;

// Decimal module class
extern PyObject *decimal_cl;

// The conversion functions
PyObject *from_value(PyObject *value);
PyObject *to_value(PyObject *bytes);

#endif // CONVERSIONS_H