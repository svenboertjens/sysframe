#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <datetime.h>
#include <ctype.h>

#include "conversions.h"

/*
  Down below are unique integer definitions, each representing a 'datachar' (datatype character).

  One datatype usually has multiple datachars, which stand for either the value type, or
  the size byte mechanism for that value type.

  With the value types, I mean different (sub)value types within a value.
  And with the size byte mechanism, I mean how the de-serialization algorithm
  knows what the byte length is of the bytes that indicate the length of the
  actual value, which comes after that. A dynamic size byte mechanism means that
  there's a single byte that represents the size of the size bytes itself, which
  then indicate the byte size of the actual value.

  The datachar definitions are sorted from 'empty object' (if there is one), to
  increasing byte sizes, so from 1 size byte, to 2 size bytes, up until the
  dynamic size bytes mechanism.

  The variables are also named after a simple structure, being the datatype in capitals
  followed by the type it stands for, such as 'STR_E' for an empty string, 'INT_1' for 
  an integer with 1 size byte, or 'BYTES_D' for a bytes object with dynamic size bytes.
  For static values, it's marked with an 'S', so for a float it's 'FLOAT_S'. Other special
  cases are explained when used.

  The global markers are used to mark certain patterns or issues during the
  convertion process. These are also used for marking the used protocol.
  The global markers count down from 255 so that new ones can be added without
  having to adjust the other markers and datachar representatives.
  The protocol markers end with the version sign, like 1 or 3 or whatever.
  The other markers have the M sign, which stands for marker.

*/

// # 'Global' markers

#define EXT_M  255 // Reserved for if we ever happen to run out of a single byte to represent stuff
#define PROT_1 254 // Protocol 1

// # 'Standard' values

// String
#define STR_E 0
#define STR_1 1
#define STR_2 2
#define STR_D 3

// Integer
#define INT_1   4 //* For integers, we don't use byte representations, as integers can be
#define INT_2   5 //  stored much more compact. Thus, INT_1 means the int value is 1 byte
#define INT_3   6 //  long, INT_2 means it's 2 bytes, etc.
#define INT_4   7 //  
#define INT_5   8 //  The dynamic method for an int uses a single byte to represent the length
#define INT_D1  9 //  at the D1, and two at the D2. An int that needs 255 bytes is already very,
#define INT_D2 10 //* very large, and a number that needs 65536 is unreachable in the practical sense.

// Float
#define FLOAT_S 11

// Boolean
#define BOOL_T 12 // Use T for True values
#define BOOL_F 13 // Use F for False values

// Complex
#define COMPLEX_S 14

// NoneType
#define NONE_S 15

// Ellipsis
#define ELLIPSIS_S 16

// Bytes
#define BYTES_E 17
#define BYTES_1 18
#define BYTES_2 19
#define BYTES_D 20

// ByteArray
#define BYTEARR_E 21
#define BYTEARR_1 22
#define BYTEARR_2 23
#define BYTEARR_D 24

// # 'List type' values

// List
#define LIST_E 25
#define LIST_1 26
#define LIST_2 27
#define LIST_D 28

// Set
#define SET_E 29
#define SET_1 30
#define SET_2 31
#define SET_D 32

// Tuple
#define TUPLE_E 33
#define TUPLE_1 34
#define TUPLE_2 35
#define TUPLE_D 36

// Dictionary
#define DICT_E 37
#define DICT_1 38
#define DICT_2 39
#define DICT_D 40

// FrozenSet
#define FSET_E 41
#define FSET_1 42
#define FSET_2 43
#define FSET_D 44

// # 'Miscellaneous' values

// DateTime
#define DATETIME_DT 45 // DT for DateTime objects
#define DATETIME_TD 46 // TD for TimeDelta objects // TODO: NOT IMPLEMENTED YET
#define DATETIME_D  47 // D  for Date objects
#define DATETIME_T  48 // T  for Time objects

// UUID
#define UUID_S 49

// MemoryView
#define MEMVIEW_E 50
#define MEMVIEW_1 51
#define MEMVIEW_2 52
#define MEMVIEW_D 53

// Decimal
#define DECIMAL_1 54
#define DECIMAL_2 55
#define DECIMAL_D 56

// Datetime module classes
PyObject *datetime_dt; // datetime
PyObject *datetime_td; // timedelta
PyObject *datetime_d;  // date
PyObject *datetime_t;  // time

// UUID module class
PyObject *uuid_cl;

// Decimal module class
PyObject *decimal_cl;

// # Helper functions for converting Py_ssize_t to PyBytes, and C bytes to size_t

static inline PyObject *unsigned_to_bytes(Py_ssize_t value)
{
    PyObject *num = PyLong_FromSsize_t(value);
    
    size_t num_bytes = (_PyLong_NumBits(num) + 7) / 8;

    unsigned char *bytes = (unsigned char *)malloc(num_bytes);

    if (_PyLong_AsByteArray((PyLongObject *)num, bytes, num_bytes, 1, 0) == -1)
    {
        free(bytes);
        return NULL;
    }

    Py_DECREF(num);

    PyObject *result = PyBytes_FromStringAndSize((const char *)bytes, num_bytes);

    free(bytes);
    return result;
}

static inline size_t bytes_to_size_t(const unsigned char *bytes, size_t length)
{
    size_t num = 0;

    for (size_t i = 0; i < length; ++i)
    {
        num += ((size_t)bytes[i]) << (i * 8);
    }

    return num;
}

// # The from-conversion functions

static inline PyObject *from_string(PyObject *value)
{
    if (!PyUnicode_Check(value))
        return NULL; // Error is set later


    PyObject *pybytes = PyUnicode_AsEncodedString(value, "utf-8", "strict");

    // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
    Py_ssize_t size = PyBytes_Size(pybytes);
    PyObject *length_bytes = unsigned_to_bytes(size);
    PyObject *metabytes; // This will hold the metadata for the bytes

    if (size == 0) // Check if it's empty
    {
        Py_DECREF(length_bytes);
        Py_XDECREF(metabytes);
        Py_DECREF(pybytes);
        // Return just the datachar for an empty string
        const unsigned char datachars[] = {STR_E, '\0'};
        return PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 256) // Check whether we can store the size in a single byte
    {
        // Get a PyBytes object of the string 1 datachar
        const unsigned char datachars[] = {STR_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 65536) // Or two bytes (2 ^ 16 - 1 = 65535)
    {
        // Get a PyBytes object of the string 2 datachar
        const unsigned char datachars[] = {STR_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else
    {
        // Get a PyBytes object of the string dynamic datachar
        const unsigned char datachars[] = {STR_D, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Get the length of the length representation bytes to allow for dynamic sizing
        PyObject *length_bytes_len = unsigned_to_bytes(PyBytes_Size(length_bytes));

        // Concat the length bytes on top
        PyBytes_ConcatAndDel(&metabytes, length_bytes_len);
    }

    // Add the length bytes and string bytes
    PyBytes_ConcatAndDel(&metabytes, length_bytes);
    PyBytes_ConcatAndDel(&metabytes, pybytes);

    return metabytes;
}

static inline PyObject *from_integer(PyObject *value)
{
    if (!PyLong_Check(value))
        return NULL;

    // Calculate number of bytes. Add 8 instead of 7 to include the sign bit
    size_t num_bytes = (_PyLong_NumBits(value) + 8) / 8;

    unsigned char *bytes = (unsigned char *)malloc(num_bytes);

    if (_PyLong_AsByteArray((PyLongObject *)value, bytes, num_bytes, 1, 0) == -1)
    {
        free(bytes);
        return NULL;
    }

    PyObject *pybytes = PyBytes_FromStringAndSize((const char *)bytes, num_bytes);

    free(bytes);

    PyObject *metabytes;
    switch (num_bytes)
    {
    case 1:
    {
        // Make a bytes object from the datachar
        const unsigned char datachars[] = {INT_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
        break;
    }
    case 2:
    {
        const unsigned char datachars[] = {INT_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
        break;
    }
    case 3:
    {
        const unsigned char datachars[] = {INT_3, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
        break;
    }
    case 4:
    {
        const unsigned char datachars[] = {INT_4, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
        break;
    }
    case 5:
    {
        const unsigned char datachars[] = {INT_5, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
        break;
    }
    default:
    {
        // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
        Py_ssize_t size = PyByteArray_Size(pybytes);
        PyObject *length_bytes = unsigned_to_bytes(size);

        // Get a PyBytes object of the integer dynamic datachar
        const unsigned char datachars[] = {num_bytes < 256 ? INT_D1 : INT_D2, '\0'}; // Add D1 for 1 byte, else D2
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Concat the length bytes on top
        PyBytes_ConcatAndDel(&metabytes, length_bytes);

        break;
    }
    }
    // Add the number bytes on top
    PyBytes_ConcatAndDel(&metabytes, pybytes);

    return metabytes;
}

inline PyObject *from_float(PyObject *value)
{
    if (!PyFloat_Check(value))
        return NULL;

    double c_num = PyFloat_AsDouble(value);
    unsigned char *bytes = (unsigned char *)malloc(sizeof(double));
    memcpy(bytes, &c_num, sizeof(double));

    PyObject *pybytes = PyBytes_FromStringAndSize((const char *)bytes, sizeof(double));

    free(bytes);

    // Get a PyBytes object of the float datachar
    const unsigned char datachars[] = {FLOAT_S, '\0'};
    PyObject *metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    // Concat the length bytes and the float bytes on top
    PyBytes_ConcatAndDel(&metabytes, pybytes);

    return metabytes;
}

static inline PyObject *from_complex(PyObject *value)
{
    if (!PyComplex_Check(value))
        return NULL;

    // Get the complex as a value
    Py_complex ccomplex = PyComplex_AsCComplex(value);

    // Allocate space for two doubles, as a complex type is basically two doubles
    unsigned char *bytes = (unsigned char *)malloc(2 * sizeof(double));

    // Copy the real and imaginary parts to the bytes object
    memcpy(bytes, &ccomplex.real, sizeof(double));
    memcpy(bytes + sizeof(double), &ccomplex.imag, sizeof(double));

    // Convert it to a PyBytes object
    PyObject *pybytes = PyBytes_FromStringAndSize((const char *)bytes, 2 * sizeof(double));

    free(bytes);

    // Only add the complex type datachar, the length is consistently 16
    const unsigned char datachars[] = {COMPLEX_S, '\0'};
    PyObject *metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    PyBytes_ConcatAndDel(&metabytes, pybytes);

    return metabytes;
}

static inline PyObject *from_boolean(PyObject *value)
{
    if (!PyBool_Check(value))
        return NULL;

    if (PyObject_IsTrue(value))
    {
        const unsigned char datachars[] = {BOOL_T, '\0'};
        return PyBytes_FromStringAndSize((const char *)datachars, 1);
    }

    const unsigned char datachars[] = {BOOL_F, '\0'};
    return PyBytes_FromStringAndSize((const char *)datachars, 1);
}

static inline PyObject *from_bytes(PyObject *value)
{
    if (!PyBytes_Check(value))
        return NULL;

    // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
    Py_ssize_t size = PyBytes_Size(value);
    PyObject *length_bytes = unsigned_to_bytes(size);
    PyObject *metabytes = NULL; // This will hold the metadata for the bytes

    if (size == 0)
    {
        Py_DECREF(length_bytes);
        Py_XDECREF(metabytes);
        // Return the datachar for an empty bytes obj
        const unsigned char datachars[] = {BYTES_E, '\0'};
        return PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 256) // Check whether we can store the size in a single byte
    {
        // Get a PyBytes object of the bytes obj 1 datachar
        const unsigned char datachars[] = {BYTES_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 65536) // Or two bytes
    {
        // Get a PyBytes object of the string 2 datachar
        const unsigned char datachars[] = {BYTES_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else
    {
        // Get a PyBytes object of the string dynamic datachar
        const unsigned char datachars[] = {BYTES_D, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Get the length of the length representation bytes to allow for dynamic sizing
        PyObject *length_bytes_len = unsigned_to_bytes(PyBytes_Size(length_bytes));

        // Concat the length bytes on top
        PyBytes_ConcatAndDel(&metabytes, length_bytes_len);
    }

    // Add the length bytes and bytes obj
    PyBytes_ConcatAndDel(&metabytes, length_bytes);
    PyBytes_Concat(&metabytes, value); // Don't delete, it's a value from the user

    return metabytes;
}

static inline PyObject *from_bytearray(PyObject *value)
{
    if (!PyByteArray_Check(value))
    {
        return NULL;
    }

    // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
    Py_ssize_t size = PyByteArray_Size(value);
    PyObject *length_bytes = unsigned_to_bytes(size);
    PyObject *metabytes = NULL; // This will hold the metadata for the bytearray

    if (size == 0)
    {
        Py_DECREF(length_bytes);
        Py_XDECREF(metabytes);
        // Return the datachar for an empty bytearray
        const unsigned char datachars[] = {BYTEARR_E, '\0'};
        return PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 256) // Check whether we can store the size in a single byte
    {
        // Get a PyBytes object of the bytearray 255 datachar
        const unsigned char datachars[] = {BYTEARR_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 65536) // Or two bytes
    {
        // Get a PyBytes object of the string 65535 datachar
        const unsigned char datachars[] = {BYTEARR_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else
    {
        // Get a PyBytes object of the string dynamic datachar
        const unsigned char datachars[] = {BYTEARR_D, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Get the length of the length representation bytes to allow for dynamic sizing
        PyObject *length_bytes_len = unsigned_to_bytes(PyBytes_Size(length_bytes));

        // Concat the length bytes on top
        PyBytes_ConcatAndDel(&metabytes, length_bytes_len);
    }
    // Add the length bytes and bytearray
    PyBytes_ConcatAndDel(&metabytes, length_bytes);
    PyBytes_Concat(&metabytes, value); // Don't delete, it's a value from the user

    return metabytes;
}

static inline PyObject *from_nonetype() // Value isn't sent as it's static, thus not required
{
    const unsigned char datachars[] = {NONE_S, '\0'};
    return PyBytes_FromStringAndSize((const char *)datachars, 1);
}

static inline PyObject *from_ellipsis()
{
    const unsigned char datachars[] = {ELLIPSIS_S, '\0'};
    return PyBytes_FromStringAndSize((const char *)datachars, 1);
}

static inline PyObject *from_datetime(PyObject *value, char *datatype) // Datetype required for the type of datetime object
{
    // This will hold the datachar
    char datachar = -1;

    // Create an iso format string of the datetime object
    PyObject *iso = PyObject_CallMethod(value, "isoformat", NULL);
    if (iso == NULL)
        return NULL;
    
    // Convert the iso string to a Pybytes object
    PyObject *bytes = PyUnicode_AsEncodedString(iso, "utf-8", "strict");

    Py_DECREF(iso);

    // Decide the datachar of the datetime object type, and do type checks
    if (strcmp("datetime.datetime", datatype) == 0)
    {
        if (!PyObject_IsInstance(value, datetime_dt))
        {
            Py_DECREF(bytes);
            PyErr_SetString(PyExc_ValueError, "Expected a datetime.datetime object.");
            return NULL;
        }
        datachar = DATETIME_DT;
    }
    else if (strcmp("datetime.date", datatype) == 0)
    {
        if (!PyObject_IsInstance(value, datetime_d))
        {
            Py_DECREF(bytes);
            return NULL;
        }
        datachar = DATETIME_D;
    }
    else if (strcmp("datetime.timedelta", datatype) == 0)
    {
        Py_DECREF(bytes);
        // TimeDelta objects aren't supported yet, mention explicitly
        PyErr_SetString(PyExc_ValueError, "DateTime.TimeDelta objects are not supported yet, though they will be later.");
        return NULL;
    }
    else if (strcmp("datetime.time", datatype) == 0)
    {
        if (!PyObject_IsInstance(value, datetime_t))
        {
            Py_DECREF(bytes);
            return NULL;
        }
        datachar = DATETIME_T;
    }
    else
    {
        Py_DECREF(bytes);
        return NULL;
    }

    // Start the bytes object off with the datachar
    PyObject *metabytes = PyBytes_FromStringAndSize(&datachar, 1);
    // Convert the size of the bytes object to bytes
    PyObject *size_bytes = unsigned_to_bytes(PyBytes_Size(bytes));

    // Add the byte objects on top of each other
    PyBytes_ConcatAndDel(&metabytes, size_bytes);
    PyBytes_ConcatAndDel(&metabytes, bytes);

    return metabytes;
}

static inline PyObject *from_decimal(PyObject *value)
{
    if (!PyObject_IsInstance(value, decimal_cl))
        return NULL;

    // Get the string representation of the Decimal object
    PyObject* str = PyObject_Str(value);
    if (str == NULL)
        return NULL;

    // Convert the string to bytes
    PyObject* bytes = PyUnicode_AsEncodedString(str, "utf-8", "strict");
    Py_DECREF(str);

    // Get the size of the bytes object as another bytes object
    Py_ssize_t num_bytes = PyBytes_Size(bytes);
    PyObject *metabytes;

    // See whether we can use the one size byte method
    if (num_bytes < 256)
    {
        const unsigned char datachars[] = {DECIMAL_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Create the size bytes and add it to the metabytes
        PyObject *size_bytes = unsigned_to_bytes(num_bytes);
        PyBytes_ConcatAndDel(&metabytes, size_bytes);
    }
    else if (num_bytes < 65536)
    {
        const unsigned char datachars[] = {DECIMAL_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Create the size bytes and add it to the metabytes
        PyObject *size_bytes = unsigned_to_bytes(num_bytes);
        PyBytes_ConcatAndDel(&metabytes, size_bytes);
    }
    else
    {
        const unsigned char datachars[] = {DECIMAL_D, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Create the size bytes, and the bytes of the size of the size bytes
        PyObject *size_bytes = unsigned_to_bytes(num_bytes);
        PyObject *size_bytes_bytes = unsigned_to_bytes(PyBytes_Size(size_bytes));

        PyBytes_ConcatAndDel(&metabytes, size_bytes_bytes);
        PyBytes_ConcatAndDel(&metabytes, size_bytes);
    }

    // Add the bytes to the metadata and return it
    PyBytes_ConcatAndDel(&metabytes, bytes);
    return metabytes;
}

static inline PyObject *from_uuid(PyObject *value)
{
    if (!PyObject_IsInstance(value, uuid_cl))
        return NULL;
    
    // Get the hexadecimal representation of the UUID
    PyObject* hex_str = PyObject_GetAttrString(value, "hex");
    if (hex_str == NULL)
    {
        Py_XDECREF(hex_str);
        PyErr_SetString(PyExc_RuntimeError, "Failed to get the UUID hex representation.");
        return NULL;
    }

    // Convert the hex string to bytes
    PyObject* bytes = PyUnicode_AsEncodedString(hex_str, "utf-8", "strict");
    if (bytes == NULL)
    {
        Py_DECREF(hex_str);
        Py_XDECREF(bytes);
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert hex string to bytes.");
        return NULL;
    }

    Py_DECREF(hex_str);

    // Start off with the UUID datachar
    const unsigned char datachars[] = {UUID_S, '\0'};
    PyObject *metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

    // Add the bytes on top and return it
    PyBytes_ConcatAndDel(&metabytes, bytes);
    return metabytes;
}

static inline PyObject *from_memoryview(PyObject *value)
{
    if (!PyMemoryView_Check(value))
        return NULL;

    // Get the underlying buffer from the memoryview
    Py_buffer* view = PyMemoryView_GET_BUFFER(value);

    // Create a bytes object from the buffer
    PyObject* bytes = PyBytes_FromStringAndSize((const char*)view->buf, view->len);
    if (bytes == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create bytes object from memoryview.");
        return NULL;
    }

    // This will hold the metabytes and the size bytes
    PyObject *metabytes;

    // Get the size of the bytes object
    Py_ssize_t size = PyBytes_Size(bytes);
    // Convert the size to bytes
    PyObject *size_bytes = unsigned_to_bytes(size);

    // Check what size the bytes object is for the suited datachar
    if (size == 0)
    {
        const unsigned char datachars[] = {MEMVIEW_E, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 256)
    {
        const unsigned char datachars[] = {MEMVIEW_1, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else if (size < 65535)
    {
        const unsigned char datachars[] = {MEMVIEW_2, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else
    {
        const unsigned char datachars[] = {MEMVIEW_D, '\0'};
        metabytes = PyBytes_FromStringAndSize((const char *)datachars, 1);

        // Get the size of the size bytes as bytes, and add it to the metabytes
        PyObject *size_bytes_bytes = unsigned_to_bytes(PyBytes_Size(size_bytes));
        PyBytes_ConcatAndDel(&metabytes, size_bytes_bytes);
    }

    // Add the bytes and return it
    PyBytes_ConcatAndDel(&metabytes, size_bytes);
    PyBytes_ConcatAndDel(&metabytes, bytes);
    return metabytes;
}

// # Functions for converting list type values to bytes and their helper functions

// Pre-definition for the items in the iterables
PyObject *from_any_value(PyObject *value);

// Function to generate the metadata of a list type based on the parsed datachars
static inline PyObject *list_type_metadata(Py_ssize_t size, char empty, char one, char two, char dynamic)
{
    // Check the size of the iterable
    if (size == 0)
    {
        // Add the datachar in a string type with a null-terminator and return it as a Python bytes object
        const unsigned char datachars[2] = {empty, '\0'};
        return PyBytes_FromStringAndSize((const char *)datachars, 1);
    }
    else
    {
        // Get the size we need to add as bytes
        PyObject *size_bytes = unsigned_to_bytes(size);
        // And the size of the size bytes
        Py_ssize_t size_bytes_size = PyBytes_Size(size_bytes);

        // This will hold the metadata
        PyObject *metadata;

        // Check which size datachar we should use now
        if (size_bytes_size == 1)
        {
            const unsigned char datachars[2] = {one, '\0'};
            metadata = PyBytes_FromStringAndSize((const char *)datachars, 1);
        }
        else if (size_bytes_size == 2)
        {
            const unsigned char datachars[2] = {two, '\0'};
            metadata = PyBytes_FromStringAndSize((const char *)datachars, 1);
        }
        else
        {
            const unsigned char datachars[2] = {dynamic, '\0'};
            metadata = PyBytes_FromStringAndSize((const char *)datachars, 1);

            // Add the size of the size bytes to the metadata as its dynamic byte size
            PyObject *size_bytes_bytes = unsigned_to_bytes(size_bytes_size);
            PyBytes_ConcatAndDel(&metadata, size_bytes_bytes);
        }

        // Add the size bytes and return the metadata
        PyBytes_ConcatAndDel(&metadata, size_bytes);
        return metadata;
    }
}

static inline PyObject *from_list(PyObject *value)
{
    // The number of items in the list
    Py_ssize_t num_items = PyList_Size(value);

    // This will be stacked onto by the item bytes
    PyObject *bytes = list_type_metadata(num_items, LIST_E, LIST_1, LIST_2, LIST_D);

    // Go over all items in the list
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the item from the list
        PyObject *item = PyList_GetItem(value, i);
        // Convert it to bytes
        PyObject *item_bytes = from_any_value(item);
        // Add it to the bytes stack
        PyBytes_ConcatAndDel(&bytes, item_bytes);
    }

    return bytes;
}

static inline PyObject *from_tuple(PyObject *value)
{
    // The number of items in the tuple
    Py_ssize_t num_items = PyTuple_Size(value);

    // This will be stacked onto by the item bytes
    PyObject *bytes = list_type_metadata(num_items, TUPLE_E, TUPLE_1, TUPLE_2, TUPLE_D);

    // Go over all items in the tuple
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the item from the list
        PyObject *item = PyTuple_GetItem(value, i);
        // Convert it to bytes
        PyObject *item_bytes = from_any_value(item);
        // Add it to the bytes stack
        PyBytes_ConcatAndDel(&bytes, item_bytes);
    }

    return bytes;
}

// Function fro both sets and frozensets
static inline PyObject *from_set_frozenset(PyObject *value, int is_frozenset)
{
    // Create an iterable from the set so that we can go over the items
    PyObject *iter = PyObject_GetIter(value);
    if (iter == NULL)
        return NULL;

    // This will hold the number of items in the iterator
    Py_ssize_t num_items = 0;
    // This will be stacked onto by the item bytes
    PyObject *bytes = PyBytes_FromStringAndSize(NULL, 0);

    // Go over all items in the set
    PyObject *item;
    while ((item = PyIter_Next(iter)) != NULL)
    {
        // Increment the number of items
        num_items++;
        
        // Convert it to bytes
        PyObject *item_bytes = from_any_value(item);
        Py_DECREF(item);

        if (item_bytes == NULL) {
            Py_DECREF(iter);
            Py_DECREF(bytes);
            return NULL;
        }

        // Add it to the bytes stack
        PyBytes_ConcatAndDel(&bytes, item_bytes);
    }

    Py_DECREF(iter);

    PyObject *metabytes;
    if (is_frozenset == 1)
    {
        metabytes = list_type_metadata(num_items, FSET_E, FSET_1, FSET_2, FSET_D);
    }
    else
    {
        metabytes = list_type_metadata(num_items, SET_E, SET_1, SET_2, SET_D);
    }

    // Add the bytes stack to the metabytes and return it
    PyBytes_ConcatAndDel(&metabytes, bytes);
    return metabytes;
}


static inline PyObject *from_dict(PyObject *value)
{
    /*
      A dict type is treated kind of as a list, but the difference
      is that it uses dict datachars and places the items from the
      dict in the order of 'key1, value1, key2, value2, etc..'.

    */

    // Get the amount of item pairs in the dict
    Py_ssize_t num_pairs = PyDict_Size(value);

    // Get the metadata for the dict and use this as the base of our bytes object
    PyObject *bytes = list_type_metadata(num_pairs, DICT_E, DICT_1, DICT_2, DICT_D);

    // Get the items of the dict in a list
    PyObject *iterable = PyDict_Items(value);

    // Go over all items in the dict
    for (Py_ssize_t i = 0; i < num_pairs; i++)
    {
        // Get the pair of the items, which is a tuple with the key on 0 and value on 1
        PyObject *pair = PyList_GetItem(iterable, i);

        // Get the key and item from the pair tuple
        PyObject *key = PyTuple_GetItem(pair, 0);
        PyObject *item = PyTuple_GetItem(pair, 1);

        // Convert the key and value to bytes
        PyObject *key_bytes = from_any_value(key);
        PyObject *item_bytes = from_any_value(item);

        // Add the key and value bytes to the byte stack
        PyBytes_ConcatAndDel(&bytes, key_bytes);
        PyBytes_ConcatAndDel(&bytes, item_bytes);
    }

    return bytes;
}

// # The main from-value converter for single values

// Replaces NULL values with a default value if applicable, else throws an error
static inline void ensure_not_null(PyObject **bytes, PyObject **default_bytes)
{
    // Return the bytes object normally if it isn't NULL
    if (*bytes != NULL)
        return;
    else
    {
        // Else check if a default value is defined
        if (*default_bytes != NULL)
        {
            // Check if the default value hasn't been converted to bytes already
            if (!PyBytes_Check(*default_bytes))
            {
                // Convert the default value to bytes and replace the default_value with it
                PyObject *converted = from_any_value(*default_bytes);
                if (converted == NULL)
                {
                    // Handle the conversion error if needed
                    return;
                }
                *default_bytes = converted;
            }

            // Assign the default bytes to the bytes object
            *bytes = *default_bytes;
            Py_INCREF(bytes);
            return;
        }
        else
        {
            // Throw an error stating the value was not supported
            PyErr_SetString(PyExc_ValueError, "Received an unsupported datatype.");
            return;
        }
    }
}

// # The main from-value conversion functions

PyObject *from_any_value(PyObject *value)
{
    // Get the datatype of the value
    const char *datatype = Py_TYPE(value)->tp_name;
    // Get the first character of the datatype
    const char datachar = *datatype;

    // This will hold the result
    PyObject *result;

    switch (datachar)
    {
    case 's': // String | Set
    {
        // Check the 2nd character
        switch (datatype[1])
        {
        case 't': return from_string(value);
        case 'e': return from_set_frozenset(value, 0);
        default:  return NULL;
        }
    }
    case 'i': return from_integer(value);
    case 'f':
    {
        switch (datatype[1])
        {
        case 'l': return from_float(value);
        case 'r': return from_set_frozenset(value, 1);
        }
    }
    return from_float(value);
    case 'c': return from_complex(value);
    case 'b': // Boolean | bytes | bytearray (all start with a 'b')
    {
        // Check the 2nd datachar
        switch (datatype[1])
        {
        case 'o': return from_boolean(value);
        default:
        {
            // Check the 5th datachar because the 2nd, 3rd, and 4th are the same
            switch (datatype[4])
            {
            case 's': return from_bytes(value);
            case 'a': return from_bytearray(value);
            default:  return NULL;
            }
        }
        }
    }
    case 'N': return from_nonetype();
    case 'e': return from_ellipsis();
    case 'd': // DateTime objects | Decimal | Dict
    {
        switch (datatype[1])
        {
        case 'a': return from_datetime(value, datatype);
        case 'e': return from_decimal(value);
        case 'i': return from_dict(value);
        default:  return NULL;
        }
    }
    case 'U': return from_uuid(value);
    case 'm': return from_memoryview(value);
    case 'l': return from_list(value);
    case 't': return from_tuple(value);
    default:  return NULL;
    }
}

PyObject *from_value(PyObject *value)
{
    PyObject *result = from_any_value(value);
    if (result == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an unsupported datatype.");
        return NULL;
    }

    // Incref for the user to hold it and return
    Py_INCREF(result);
    return result;
}

// # Helper functions for the to-conversion functions

// This struct holds the bytes and its current offset
typedef struct {
    size_t offset;
    size_t max_offset;
    const unsigned char *bytes;
} ByteData;

// Function to check whether the offset is still correct after going up by argument 'jump'. Returns -1 on failure
static inline int ensure_offset(ByteData *bd, size_t jump)
{
    /*
      This function will be used in all to-conversion functions to ensure the
      max offset is not exceeded. It'll be used like this:

      `if (ensure_offset(bd, n) == -1) return NULL;`

      Where `n` is the amount of bytes we will attempt to read later, the 'jump'.

    */

    // Check whether the offset plus the jump exceeds the max offset
    if (bd->offset + jump > bd->max_offset)
    {
        printf("%zu > %zu\n", bd->offset + jump, bd->max_offset);
        // Set an error and return -1
        PyErr_SetString(PyExc_ValueError, "Likely received an invalid bytes object: offset exceeded max limit.");
        return -1;
    }
    else
    {
        // Return 1 to state nothing is wrong
        return 1;
    }
}

// # The to-conversion functions

static inline PyObject *to_str_e(ByteData *bd) // bd is short for bytedata
{
    // Ensure we can increment the offset
    if (ensure_offset(bd, 1) == -1) return NULL;

    // Increment the offset by 1
    bd->offset++;
    // Return an empty string
    return PyUnicode_FromStringAndSize(NULL, 0);
}

// Generic function for converting strings with any size bytes length
static inline PyObject *to_str_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the length of the item bytes
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    if (ensure_offset(bd, length) == -1) return NULL;

    // Copy the value bytes into a PyBytes object
    PyObject *item_bytes = PyBytes_FromStringAndSize((const char *)(&(bd->bytes[bd->offset])), length);

    // Update the offset to start at the next item
    bd->offset += length;

    // Get the value of the bytes
    PyObject *value = PyUnicode_FromEncodedObject(item_bytes, "utf-8", "strict");

    Py_DECREF(item_bytes);

    return value;
}

// Generic function for ints
static inline PyObject *to_int_gen(ByteData *bd, size_t length)
{
    if (ensure_offset(bd, length + 1) == -1) return NULL;

    // Create a Python integer from the bytes object
    PyObject *value = _PyLong_FromByteArray((const char *)(&(bd->bytes[++bd->offset])), length, 1, 1);

    // Update the offset to start at the next item
    bd->offset += length;
    return value;
}

static inline PyObject *to_float_s(ByteData *bd)
{
    if (ensure_offset(bd, (size_t)sizeof(double) + 1) == -1) return NULL;

    // Set up a double and copy the bytes to it
    double value;
    memcpy(&value, &(bd->bytes[++bd->offset]), sizeof(double));

    // Update the offset to start at the next item
    bd->offset += sizeof(double);
    // Convert the double to a Python float and return it
    return PyFloat_FromDouble(value);
}

// Generic method to convert bool values
static inline PyObject *to_bool_gen(ByteData *bd, PyObject *bool_value)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    // Increment once to skip over the datachar
    bd->offset++;
    return bool_value;
}

static inline PyObject *to_complex_s(ByteData *bd)
{
    if (ensure_offset(bd, (size_t)sizeof(double) * 2 + 1) == -1) return NULL;

    double real, imag;
    memcpy(&real, &(bd->bytes[++bd->offset]), sizeof(double));
    memcpy(&imag, &(bd->bytes[bd->offset + sizeof(double)]), sizeof(double));

    // Update the offset to start at the next items bytes
    bd->offset += 2 * sizeof(double);

    // Create a complex object and set the real and imaginary values
    Py_complex c;
    c.real = real;
    c.imag = imag;

    // Return the complex object as a PyComplex
    return PyComplex_FromCComplex(c);
}

static inline PyObject *to_none_s(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    return Py_None;
}

static inline PyObject *to_ellipsis_s(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    return Py_Ellipsis;
}

// This function works for both regular bytes and a bytearray
static inline PyObject *to_bytes_e(ByteData *bd, int is_bytearray)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    // Create and return an empty bytes object
    return is_bytearray ? PyBytes_FromStringAndSize(NULL, 0) : PyByteArray_FromStringAndSize(NULL, 0);
}

// Generic method for bytes/bytearray conversion
static inline PyObject *to_bytes_gen(ByteData *bd, size_t size_bytes_length, int is_bytearray)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the length of the actual value
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    if (ensure_offset(bd, length) == -1) return NULL;

    // Get the actual value bytes using the length. Create a PyBytes object if is_bytearray is 0, else a PyByteArray
    PyObject *item_bytes = is_bytearray == 0 ? PyBytes_FromStringAndSize((const char *)(&(bd->bytes[bd->offset])), length) : PyByteArray_FromStringAndSize((const char *)(&(bd->bytes[bd->offset])), length);
    bd->offset += length;

    // Return the item bytes as it's supposed to be a bytes object
    return item_bytes;
}

// Generic method for datetime object conversion
static inline PyObject *to_datetime_gen(ByteData *bd, PyObject *method)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    // Get the length of the datetime bytes
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);

    if (ensure_offset(bd, length + 1) == -1) return NULL;
    
    // Grab the iso string from the C bytes
    PyObject *iso_bytes =  PyBytes_FromStringAndSize((const char *)(&(bd->bytes[++bd->offset])), length);
    bd->offset += length;

    // Get the iso string back
    PyObject *iso = PyUnicode_FromEncodedObject(iso_bytes, "utf-8", "strict");

    // Convert the iso string back to a datetime object
    PyObject *datetime_obj = PyObject_CallMethod(method, "fromisoformat", "O", iso); // The parsed method is the class to call

    Py_DECREF(iso_bytes);
    Py_DECREF(iso);

    return datetime_obj;
}

static inline PyObject *to_uuid_s(ByteData *bd)
{
    if (ensure_offset(bd, 33) == -1) return NULL;

    // Convert the bytes back to a uuid object
    PyObject* uuid = PyObject_CallFunction(uuid_cl, "O", PyUnicode_FromStringAndSize((const char *)(&(bd->bytes[++bd->offset])), 32)); // 32 because UUIDs are always of that length
    if (uuid == NULL)
    {
        Py_XDECREF(uuid);
        PyErr_SetString(PyExc_RuntimeError, "Failed to create UUID object.");
        return NULL;
    }

    // Increment the size after type check as it's unnecessary when the type check fails
    bd->offset += 32;

    return uuid;
}

static inline PyObject *to_memoryview_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    // Create and return an empty memoryview object using an empty bytes object
    PyObject *empty_obj = PyBytes_FromStringAndSize(NULL, 0);
    PyObject *memoryview = PyMemoryView_FromObject(empty_obj);
    Py_DECREF(empty_obj);
    return memoryview;
}

// Generic method for memoryview conversion
static inline PyObject *to_memoryview_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the size of the object
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    if (ensure_offset(bd, length) == -1) return NULL;

    // Create a bytes object out of the memoryview buffer bytes
    PyObject *buf_bytes = PyBytes_FromStringAndSize((const char *)(&(bd->bytes[bd->offset])), length);

    // Create a memoryview object out of the buffer
    PyObject *memoryview = PyMemoryView_FromObject(buf_bytes);
    Py_DECREF(buf_bytes);
    if (!memoryview)
    {
        Py_XDECREF(memoryview);
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert a memoryview buffer to bytes.");
        return NULL;
    }

    bd->offset += length;

    return memoryview;
}

static inline PyObject *to_decimal_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the size of the value
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    if (ensure_offset(bd, length) == -1) return NULL;

    // Get the bytes of the decimal
    PyObject *decimal_str = PyUnicode_FromStringAndSize((const char *)(&(bd->bytes[bd->offset])), length);

    // Convert it to a decimal
    PyObject *args = PyTuple_Pack(1, decimal_str);
    PyObject *decimal = PyObject_CallFunction(decimal_cl, "O", decimal_str);
    Py_DECREF(args);
    if (decimal == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert string to Decimal.");
        return NULL;
    }

    bd->offset += length;

    return decimal;
}

// # The list type conversion functions and their helper functions

// Pre-definition of the global conversion function to use it in list type functions
static inline PyObject *to_any_value(ByteData *bd);

static inline PyObject *to_list_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    // Return an empty Python list
    return PyList_New(0);
}

static inline PyObject *to_list_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the number of items in the list
    size_t num_items = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    // Create a list with the amount of items we expect
    PyObject *list = PyList_New(num_items);

    // Go over each item and add them to the list
    for (size_t i = 0; i < num_items; i++)
    {
        PyObject *item = to_any_value(bd);

        // Check if the item actually exists
        if (item == NULL)
        {
            Py_DECREF(list);
            // The error message has already been set
            return NULL;
        }

        // Append the item and continue
        Py_INCREF(item);
        PyList_SET_ITEM(list, i,  item);
    }

    return list;
}

static inline PyObject *to_tuple_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    // Return an empty Python tuple
    return PyTuple_New(0);
}

static inline PyObject *to_tuple_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the number of items in the tuple
    size_t num_items = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    // Create a tuple with the amount of items we expect
    PyObject *tuple = PyTuple_New(num_items);

    // Go over each item and add them to the tuple
    for (Py_ssize_t i = 0; i < (Py_ssize_t)num_items; i++)
    {
        PyObject *item = to_any_value(bd);

        // Check if the item actually exists
        if (item == NULL)
        {
            Py_DECREF(tuple);
            // The error message has already been set
            return NULL;
        }

        // Add the item and continue
        PyTuple_SET_ITEM(tuple, i, item);
    }

    return tuple;
}

// This function can be used for both regular and frozen sets
static inline PyObject *to_set_frozenset_e(ByteData *bd, int is_frozenset)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;

    // Create an empty list to create the set out of
    PyObject *empty_list = PyList_New(0);

    // Create the correct set type out of the empty list
    PyObject *set_type =  is_frozenset == 0 ? PySet_New(empty_list) : PyFrozenSet_New(empty_list);

    Py_DECREF(empty_list);
    return set_type;
}

// Also for both regular and frozen sets
static inline PyObject *to_set_frozenset_gen(ByteData *bd, size_t size_bytes_length, int is_frozenset)
{
    // No need to ensure the offset, that'll happen in the list function

    // A set is created out of a different iterable, so create a list and use that
    PyObject *list = to_list_gen(bd, size_bytes_length);
    // Check if actually exists
    if (list == NULL)
    {
        // Error message already set
        return NULL;
    }
    
    // Create the set type out of the created list
    PyObject *set_type =  is_frozenset == 0 ? PySet_New(list) : PyFrozenSet_New(list);

    Py_DECREF(list);
    return set_type;
}

static inline PyObject *to_dict_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;
    // Create and return an empty dict object
    return PyDict_New();
}

static inline PyObject *to_dict_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the number of pairs in the dict
    size_t num_items = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    // Create a n empty dict object
    PyObject *dict = PyDict_New();

    // Go over each pair and add them to the dict
    for (size_t i = 0; i < num_items; i++)
    {
        // Create the key and the value, placed directly after each other
        PyObject *key = to_any_value(bd);
        PyObject *value = to_any_value(bd);

        // Check if both items actually exist
        if (key == NULL || value == NULL)
        {
            Py_DECREF(dict);
            Py_XDECREF(key);
            Py_XDECREF(value);
            // The error message has already been set
            return NULL;
        }

        // Place the key-value pair in the dict
        PyDict_SetItem(dict, key, value);
    }

    return dict;
}

static inline PyObject *to_any_value(ByteData *bd)
{
    // Get the datachar of the current value and switch over it
    const unsigned char datachar = bd->bytes[bd->offset];

    switch (datachar)
    {
    case STR_E: return to_str_e(bd);
    case STR_1: return to_str_gen(bd, 1);
    case STR_2: return to_str_gen(bd, 2);
    case STR_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the length bytes from the 1st character away from the offset
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic to_str method
        return to_str_gen(bd, size_bytes_length);
    }
    case INT_D1:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic to_int method
        return to_int_gen(bd, length);
    }
    case INT_D2:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Increment once more to start at the int value bytes
        bd->offset++;
        // Use and return the generic to_int method
        return to_int_gen(bd, length);
    }
    case INT_1:       return to_int_gen(bd, 1);
    case INT_2:       return to_int_gen(bd, 2);
    case INT_3:       return to_int_gen(bd, 3);
    case INT_4:       return to_int_gen(bd, 4);
    case INT_5:       return to_int_gen(bd, 5);
    case FLOAT_S:     return to_float_s(bd);
    case BOOL_T:      return to_bool_gen(bd, Py_True);
    case BOOL_F:      return to_bool_gen(bd, Py_False);
    case COMPLEX_S:   return to_complex_s(bd);
    case NONE_S:      return to_none_s(bd);
    case ELLIPSIS_S:  return to_ellipsis_s(bd);
    case BYTES_E:     return to_bytes_e(bd, 0);
    case BYTES_1:     return to_bytes_gen(bd, 1, 0);
    case BYTES_2:     return to_bytes_gen(bd, 2, 0);
    case BYTES_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the length bytes from the 1st character away from the offset
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_bytes_gen(bd, size_bytes_length, 0);
    }
    case BYTEARR_E:   return to_bytes_e(bd, 1);
    case BYTEARR_1:   return to_bytes_gen(bd, 1, 1);
    case BYTEARR_2:   return to_bytes_gen(bd, 2, 1);
    case BYTEARR_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the length bytes from the 1st character away from the offset
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_bytes_gen(bd, size_bytes_length, 1);
    }
    case DATETIME_DT: return to_datetime_gen(bd, datetime_dt);
    case DATETIME_D:  return to_datetime_gen(bd, datetime_d); 
    case DATETIME_T:  return to_datetime_gen(bd, datetime_t);
    case UUID_S:      return to_uuid_s(bd);
    case MEMVIEW_E:   return to_memoryview_e(bd);
    case MEMVIEW_1:   return to_memoryview_gen(bd, 1);
    case MEMVIEW_2:   return to_memoryview_gen(bd, 2);
    case MEMVIEW_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the size of the size bytes
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_memoryview_gen(bd, size_bytes_length);
    }
    case DECIMAL_1:   return to_decimal_gen(bd, 1);
    case DECIMAL_2:   return to_decimal_gen(bd, 2);
    case DECIMAL_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the size of the size bytes
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_decimal_gen(bd, size_bytes_length);
    }
    case LIST_E:      return to_list_e(bd);
    case LIST_1:      return to_list_gen(bd, 1);
    case LIST_2:      return to_list_gen(bd, 2);
    case LIST_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_list_gen(bd, size_bytes_length);
    }
    case TUPLE_E:     return to_tuple_e(bd);
    case TUPLE_1:     return to_tuple_gen(bd, 1);
    case TUPLE_2:     return to_tuple_gen(bd, 2);
    case TUPLE_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_tuple_gen(bd, size_bytes_length);
    }
    case SET_E:       return to_set_frozenset_e(bd, 0);
    case SET_1:       return to_set_frozenset_gen(bd, 1, 0);
    case SET_2:       return to_set_frozenset_gen(bd, 2, 0);
    case SET_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_set_frozenset_gen(bd, size_bytes_length, 0);
    }
    case FSET_E:      return to_set_frozenset_e(bd, 1);
    case FSET_1:      return to_set_frozenset_gen(bd, 1, 1);
    case FSET_2:      return to_set_frozenset_gen(bd, 2, 1);
    case FSET_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_set_frozenset_gen(bd, size_bytes_length, 1);
    }
    case DICT_E:      return to_dict_e(bd);
    case DICT_1:      return to_dict_gen(bd, 1);
    case DICT_2:      return to_dict_gen(bd, 2);
    case DICT_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_dict_gen(bd, size_bytes_length);
    }
    default:
    {
        // Invalid datachar received
        PyErr_Format(PyExc_ValueError, "Likely received an invalid bytes object: fetched an invalid datatype representative. (Rep. code: %i)", (int)datachar);
        return NULL;
    }
    }
}

// # The main to-value conversion function

PyObject *to_value(PyObject *py_bytes)
{
    // Get the PyBytes object as a C string
    const unsigned char *bytes = PyBytes_AsString(py_bytes);
    // Get the length of this bytes object for memory allocation
    Py_ssize_t bytes_length = PyBytes_Size(py_bytes);

    // Create the bytedata struct
    ByteData *bd = (ByteData *)malloc(sizeof(ByteData));
    if (bd == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    // Allocate the space for the bytes object
    bd->bytes = (const unsigned char *)malloc(bytes_length);
    if (bd->bytes == NULL)
    {
        free(bd);
        PyErr_NoMemory();
        return NULL;
    }

    // Write the bytes and offset to the bytedata
    memcpy(bd->bytes, bytes, (size_t)bytes_length);
    bd->offset = 0; // Start at offset 0, which is the start of the bytes object
    bd->max_offset = (size_t)bytes_length; // Set the max offset to the bytes length

    // Use and return the to-any-value conversion function
    PyObject *result = to_any_value(bd);
    free(bd->bytes);
    free(bd);
    
    if (result == NULL)
        return NULL;

    Py_INCREF(result);
    return result;
}

