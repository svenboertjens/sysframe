#ifndef SBS_2_H
#define SBS_2_H

// Python define statement
#define PY_SSIZE_T_CLEAN

// Includes
#include <Python.h>
#include <datetime.h>
#include <ctype.h>

// Datetime module classes
extern PyObject *datetime_dt; // datetime
extern PyObject *datetime_d;  // date
extern PyObject *datetime_t;  // time
// UUID module class
extern PyObject *uuid_cl;
// Decimal module class
extern PyObject *decimal_cl;
// Namedtuple module class
extern PyObject *namedtuple_cl;

// Initialize the SBS module
int sbs2_init(void);
// Cleanup the SBS module
void sbs2_cleanup(void);

// Convert a value to bytes
PyObject *from_value(PyObject *value);
// Convert a bytes object to the value it used to be
PyObject *to_value(PyObject *bytes);

#endif // SBS_2_H