#include "sbs_2.h"
#include "sbs_old/sbs_1.h"

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
    // Cleanup the SBS modules
    sbs2_cleanup(); // Just the current one, the values to clean up are shared accross all SBS files

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

    // Init the SBS modules
    if (sbs2_init() == -1) return NULL; // Only the current SBS protocol can produce errors
    sbs1_init();

    // Create and return this module
    return PyModule_Create(&pybytes);
}

