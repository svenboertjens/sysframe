#include "conversions.h"

// # The python handles for from and to value calls

static PyObject *py_from_value(PyObject *self, PyObject *args)
{
    PyObject *value;

    // Parse the args and kwargs
    if (!PyArg_ParseTuple(args, "O", &value))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'any' argument.");
        return NULL;
    }

    Py_INCREF(value);

    // Call the imported from_value converter function
    PyObject *bytes = from_value(value);

    // Clean up reference
    Py_DECREF(value);

    return bytes;
}

static PyObject *py_to_value(PyObject *self, PyObject *args)
{
    PyObject *py_bytes = NULL;

    if (!PyArg_ParseTuple(args, "O!", &PyBytes_Type, &py_bytes))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'bytes' type.");
        return NULL;
    }

    Py_INCREF(py_bytes);

    PyObject *result = to_value(py_bytes);

    Py_DECREF(py_bytes);
    return result;
}

// # Module declarations

// The offered methods and their descriptions
static PyMethodDef methods[] = {
    {"from_value", py_from_value, METH_VARARGS, "Convert a value to a bytes object."},
    {"to_value", py_to_value, METH_VARARGS, "Convert a bytes object to a value."},

    {NULL, NULL, 0, NULL}
};

// Finalize the Python interpreter on exit
void pybytes_module_cleanup(void *module)
{
    // Dereference the global module variables
    Py_XDECREF(datetime_dt);
    Py_XDECREF(datetime_td);
    Py_XDECREF(datetime_d);
    Py_XDECREF(datetime_t);
    Py_XDECREF(uuid_cl);
    Py_XDECREF(decimal_cl);

    // Close the Python interpreter
    Py_Finalize();
}

// Information about this Python module
static struct PyModuleDef pybytes = {
    PyModuleDef_HEAD_INIT,
    "pybytes",
    "A module for converting values to bytes and vice versa.",
    -1,
    methods,
    NULL, NULL, NULL,
    pybytes_module_cleanup
};

// Initialization function
PyMODINIT_FUNC PyInit_pybytes(void)
{
    // Init the Python interpreter
    Py_Initialize();

    // Import the datetime module
    PyDateTime_IMPORT;

    // Get the datetime module
    PyObject *datetime_m = PyImport_ImportModule("datetime");
    if (datetime_m == NULL)
    {
        // Datetime module was not found
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'datetime'.");
        return NULL;
    }

    // Get the required datetime attributes
    datetime_dt = PyObject_GetAttrString(datetime_m, "datetime");
    datetime_td = PyObject_GetAttrString(datetime_m, "timedelta");
    datetime_d = PyObject_GetAttrString(datetime_m, "date");
    datetime_t = PyObject_GetAttrString(datetime_m, "time");

    // Check whether the attributes aren't NULL
    if (datetime_dt == NULL)
    {
        // Cleanup of attributes is not necessary here, that's done on process exit in the finalize function
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'datetime' in module 'datetime'.");
        return NULL;
    }
    if (datetime_td == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'timedelta' in module 'datetime'.");
        return NULL;
    }
    if (datetime_d == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'date' in module 'datetime'.");
        return NULL;
    }
    if (datetime_t == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'time' in module 'datetime'.");
        return NULL;
    }

    Py_DECREF(datetime_m);

    // Get the UUID module
    PyObject* uuid_m = PyImport_ImportModule("uuid");
    if (uuid_m == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'uuid'.");
        return NULL;
    }

    // Get the required attribute from the module
    uuid_cl = PyObject_GetAttrString(uuid_m, "UUID");

    Py_DECREF(uuid_m);

    // Check whether the attribute isn't NULL
    if (uuid_cl == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'UUID' in module 'uuid'.");
        return NULL;
    }

    // Get the decimal module
    PyObject *decimal_m = PyImport_ImportModule("decimal");
    if (decimal_m == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'decimal'.");
        return NULL;
    }

    // Get the required attribute from the module
    decimal_cl = PyObject_GetAttrString(decimal_m, "Decimal");

    Py_DECREF(decimal_m);

    // Check whether the attribute isn't NULL
    if (decimal_cl == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'Decimal' in module 'decimal'.");
        return NULL;
    }

    // Create and return this module
    return PyModule_Create(&pybytes);
}

