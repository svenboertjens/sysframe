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

  Apart from the 'standard' serialization, this module also supports a custom SFS
  protocol. SFS stands for Simple File System, as it works similarly to one, and
  is just a simplified version of one. This allows for manipulation of a bytestream
  without having to de-serialize it completely, manipulating the value, and then
  serializing it again. This uses a mapping format which is used to locate the
  item to modify, so that it can be pulled. modified, or removed.

*/

// # 'Global' markers

#define EXT_M      255 // Reserved for if we ever happen to run out of a single byte to represent stuff
#define PROT_STD_1 254 // Protocol 1, standard
#define PROT_SFS_1 253 // Protocol 1, SFS (Simple File System)

#define PROT_STD_D PROT_STD_1 // The default STD protocol
#define PROT_SFS_D PROT_SFS_1 // The default SFS protocol

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
#define INT_D2 10 //* very large, and a number that needs 65536 is practically unreachable.

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

/*
  # Datatypes to add:
  
  - NamedTuple
  - Range
  - Deque
  - DefaultDict
  - OrderedDict
  - Enum
  - Pathlib

*/

// # Return status codes

typedef enum {
    SC_SUCCESS     = 0, // Successful operation, no issues
    SC_INCORRECT   = 1, // Incorrect datatype received
    SC_UNSUPPORTED = 2, // Unsupported datatype received
    SC_EXCEPTION   = 3, // Exception where an error is set by the returner
    SC_NESTDEPTH   = 4, // Nesting depth is deeper than allowed
    SC_NOMEMORY    = 5  // Not enough memory to do an operation
} StatusCode;

// # Other definitions

#define ALLOC_SIZE 128 // The size to add when (re)allocating space for bytes
#define MAX_NESTS  51  // The maximun amount of nests allowed, plus 1

// Sys getsizeof class
PyObject *sys_cl;

// Datetime module classes
PyObject *datetime_dt; // datetime
PyObject *datetime_d;  // date
PyObject *datetime_t;  // time

// UUID module class
PyObject *uuid_cl;

// Decimal module class
PyObject *decimal_cl;

// # Initialization and cleanup functions

int conversions_init(void)
{
    // Get the sys module
    PyObject *sys_m = PyImport_ImportModule("sys");
    if (sys_m == NULL)
    {
        // Datetime module was not found
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'sys'.");
        return -1;
    }

    // Get the sys getsizeof class
    sys_cl = PyObject_GetAttrString(sys_m, "getsizeof");

    Py_DECREF(sys_m);

    // Check whether the class isn't NULL
    if (sys_cl == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'getsizeof' in module 'sys'.");
        return -1;
    }

    // Import the datetime module
    PyDateTime_IMPORT;

    // Get the datetime module
    PyObject *datetime_m = PyImport_ImportModule("datetime");
    if (datetime_m == NULL)
    {
        // Datetime module was not found
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'datetime'.");
        return -1;
    }

    // Get the required datetime attributes
    datetime_dt = PyObject_GetAttrString(datetime_m, "datetime");
    datetime_d = PyObject_GetAttrString(datetime_m, "date");
    datetime_t = PyObject_GetAttrString(datetime_m, "time");

    // Check whether the attributes aren't NULL
    if (datetime_dt == NULL)
    {
        // Cleanup of attributes is not necessary here, that's done on process exit in the finalize function
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'datetime' in module 'datetime'.");
        return -1;
    }
    if (datetime_d == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'date' in module 'datetime'.");
        return -1;
    }
    if (datetime_t == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'time' in module 'datetime'.");
        return -1;
    }

    Py_DECREF(datetime_m);

    // Get the UUID module
    PyObject* uuid_m = PyImport_ImportModule("uuid");
    if (uuid_m == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'uuid'.");
        return -1;
    }

    // Get the required attribute from the module
    uuid_cl = PyObject_GetAttrString(uuid_m, "UUID");

    Py_DECREF(uuid_m);

    // Check whether the attribute isn't NULL
    if (uuid_cl == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find attribute 'UUID' in module 'uuid'.");
        return -1;
    }

    // Get the decimal module
    PyObject *decimal_m = PyImport_ImportModule("decimal");

    if (decimal_m == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'decimal'.");
        return -1;
    }

    // Get the required attribute from the module
    decimal_cl = PyObject_GetAttrString(decimal_m, "Decimal");

    Py_DECREF(decimal_m);

    // Check whether the attribute isn't NULL
    if (decimal_cl == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'Decimal' in module 'decimal'.");
        return -1;
    }

    return 1;
}

void conversions_cleanup(void)
{
    // Decref all modules with XDECREF because they are NULL if not found
    Py_XDECREF(sys_cl);
    Py_XDECREF(datetime_dt);
    Py_XDECREF(datetime_d);
    Py_XDECREF(datetime_t);
    Py_XDECREF(uuid_cl);
    Py_XDECREF(decimal_cl);
}

// # Helper functions for the from-conversion functions

// Struct that holds the values converted to C bytes
typedef struct {
    Py_ssize_t offset;
    Py_ssize_t max_size;
    int nests;
    unsigned char *bytes;
} ValueData;

// This function resizes the bytes of the ValueData when necessary
static inline StatusCode auto_resize_vd(ValueData *vd, Py_ssize_t jump)
{
    // Check if we need to reallocate for more space with the given jump
    if (vd->offset + jump > vd->max_size)
    {
        // Update the max size
        vd->max_size += jump + ALLOC_SIZE;
        // Reallocate to the new max size
        unsigned char *temp = (unsigned char *)realloc((void *)(vd->bytes), vd->max_size * sizeof(unsigned char));
        if (temp == NULL) return SC_NOMEMORY; // Return no memory issue

        // Update the bytes to point to the new allocated bytes
        vd->bytes = temp;
    }

    // Return success
    return SC_SUCCESS;
}

// This function updates the nest depth and whether we've reached the max nests
static inline StatusCode increment_nests(ValueData *vd)
{
    // Increment the nest depth
    vd->nests++;
    // Check whether we reached the max nest depth
    if (vd->nests == MAX_NESTS)
        // Return nesting issue status
        return SC_NESTDEPTH;
    
    // Return success
    return SC_SUCCESS;
}

// Function to initiate the ValueData class
static inline ValueData init_vd(PyObject *value, StatusCode *status)
{
    // Attempt to estimate what the max possible byte size will be using sys.getsizeof
    PyObject *max_size_num = PyObject_CallFunctionObjArgs(sys_cl, value);
    Py_ssize_t max_size = PyLong_AsSsize_t(max_size_num) + ALLOC_SIZE; // Add the alloc size as headroom

    Py_DECREF(max_size_num);

    // Create the struct itself
    ValueData vd = {1, max_size, 0, (unsigned char *)malloc(max_size * sizeof(unsigned char))};
    if (vd.bytes == NULL)
    {
        // Set the status
        *status = SC_NOMEMORY;
        return vd;
    }

    // Write the protocol byte
    const unsigned char protocol = PROT_STD_D;
    vd.bytes[0] = protocol;

    *status = SC_SUCCESS;

    return vd;
}

// Function to calculate the number of bytes needed to represent a Py_ssize_t
static inline Py_ssize_t get_num_bytes(Py_ssize_t value)
{
    // Determine the number of bytes needed
    Py_ssize_t num_bytes = 0;
    Py_ssize_t temp = value;
    while (temp > 0) {
        num_bytes++;
        temp >>= 8;
    }

    return num_bytes;
}

// Function to write the datachar and the size as bytes
static inline StatusCode write_metadata(ValueData *vd, const unsigned char datachar, Py_ssize_t value, Py_ssize_t num_bytes) {
    // Write the datachar
    vd->bytes[vd->offset++] = datachar;

    // Write the size as bytes directly
    for (Py_ssize_t i = 0; i < num_bytes; i++) {
        vd->bytes[vd->offset++] = (unsigned char)(value & 0xFF);
        value >>= 8;
    }

    return SC_SUCCESS;
}

// Alike the regular write-metadata function, but also writes the dynamic datachar
static inline StatusCode write_dynamic_metadata(ValueData *vd, const unsigned char datachar, Py_ssize_t value, Py_ssize_t num_bytes) {
    // Resize if necessary
    if (auto_resize_vd(vd, num_bytes + 2) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
    vd->bytes[vd->offset++] = datachar;
    // Write the dynamic size byte
    vd->bytes[vd->offset++] = (const unsigned char)num_bytes;

    // Write the size as bytes directly
    for (Py_ssize_t i = 0; i < num_bytes; i++) {
        vd->bytes[vd->offset + i] = (unsigned char)(value & 0xFF);
        value >>= 8;
    }

    // Update the offset
    vd->offset += num_bytes;

    return SC_SUCCESS;
}

// Function to write the full data with an E-1-2-D setup
static inline StatusCode write_E12D(ValueData *vd, Py_ssize_t size, const unsigned char *bytes, const unsigned char empty)
{
    Py_ssize_t num_bytes = get_num_bytes(size);

    // Check if we can use regular datachars or have to use the dynamic datachar
    switch (num_bytes)
    {
    case 0:
    {
        // Resize for a single byte
        if (auto_resize_vd(vd, 1) == SC_NOMEMORY) return SC_NOMEMORY;
        // Write the empty datachar
        vd->bytes[vd->offset++] = empty;

        // Return success directly to not add the value bytes, as those are empty
        return SC_SUCCESS;
    }
    case 1:
    case 2:
    {
        // Resize if necessary
        if (auto_resize_vd(vd, num_bytes + size + 1) == SC_NOMEMORY) return SC_NOMEMORY;

        // Write the metadata and set the datachar to the empty one plus the offset, to get the 1 or 2 case datachar
        if (write_metadata(vd, empty + num_bytes, size, num_bytes) == SC_NOMEMORY) return SC_NOMEMORY;
        break;
    }
    default:
    {
        // Resize if necessary
        if (auto_resize_vd(vd, num_bytes + size + 1) == SC_NOMEMORY) return SC_NOMEMORY;
        // Write the dynamic metadata
        if (write_dynamic_metadata(vd, empty + 3, size, num_bytes) == SC_NOMEMORY) return SC_NOMEMORY;
        break;
    }
    }

    // Check if we should add the bytes
    if (bytes != NULL)
    {
        // Copy the bytes of the value to the bytes stack
        memcpy(&(vd->bytes[vd->offset]), bytes, size);
        vd->offset += size;
    }
    

    return SC_SUCCESS;
}

// Function to convert C bytes to a size_t
static inline size_t bytes_to_size_t(const unsigned char *bytes, size_t length)
{
    size_t num = 0;

    // Convert the byte array back to a size_t value (little-endian)
    for (size_t i = 0; i < length; ++i)
    {
        num |= ((size_t)bytes[i]) << (i * 8);
    }

    return num;
}

// # The from-conversion functions

static inline StatusCode from_string(ValueData *vd, PyObject *value) // VD is short for ValueData
{
    if (!PyUnicode_Check(value)) return SC_INCORRECT;
    
    // Get the string as C bytes and get its size
    Py_ssize_t size;
    const char *bytes = PyUnicode_AsUTF8AndSize(value, &size);

    // Write the data
    if (write_E12D(vd, size, (const unsigned char *)bytes, STR_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Return success
    return SC_SUCCESS;
}

static inline StatusCode from_integer(ValueData *vd, PyObject *value)
{
    if (!PyLong_Check(value)) return SC_INCORRECT;

    // Calculate number of bytes needed, including the sign bit
    size_t num_bytes = (Py_SIZE(value) > 0) ? ((_PyLong_NumBits(value) + 8) / 8) : 1;

    // Determine datachar and dynamic length
    unsigned char datachar;
    int is_dynamic = 0;

    if (num_bytes <= 5)
    {
        datachar = INT_1 + (num_bytes - 1);
    }
    else
    {
        datachar = num_bytes < 256 ? INT_D1 : INT_D2;
        is_dynamic = num_bytes < 256 ? 1 : 2;
    }
    
    // Resize if necessary
    if (auto_resize_vd(vd, 1 + is_dynamic + num_bytes) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
    vd->bytes[vd->offset++] = datachar;

    // Write dynamic size bytes if necessary
    if (is_dynamic == 1)
    {
        vd->bytes[vd->offset++] = (const unsigned char)num_bytes;
    }
    else if (is_dynamic == 2)
    {
        vd->bytes[vd->offset++] = (const unsigned char)(num_bytes & 0xFF);
        num_bytes >>= 8;
        vd->bytes[vd->offset++] = (const unsigned char)(num_bytes & 0xFF);
    }

    // Write the bytes directly to the bytes stack instead of allocating a bytes object and using write_vd
    if (_PyLong_AsByteArray((PyLongObject *)value, &(vd->bytes[vd->offset]), num_bytes, 1, 1) == -1) return SC_INCORRECT;

    // Update the offset
    vd->offset += num_bytes;

    return SC_SUCCESS;
}

static inline StatusCode from_float(ValueData *vd, PyObject *value)
{
    if (!PyFloat_Check(value)) return SC_INCORRECT;

    // Resize if necessary
    if (auto_resize_vd(vd, 1 + sizeof(double)) == SC_NOMEMORY) return SC_NOMEMORY;

    // Assign the datachar
    vd->bytes[vd->offset++] = FLOAT_S;

    // Write the float converted to a double to the bytes
    double c_num = PyFloat_AsDouble(value);
    memcpy(&(vd->bytes[vd->offset]), &c_num, sizeof(double));
    vd->offset += sizeof(double); // Update the offset

    return SC_SUCCESS;
}

static inline StatusCode from_complex(ValueData *vd, PyObject *value)
{
    if (!PyComplex_Check(value)) return SC_INCORRECT;

    // Resize if necessary
    if (auto_resize_vd(vd, 1 + (2 * sizeof(double))) == SC_NOMEMORY) return SC_NOMEMORY;

    // Get the complex as a value
    Py_complex ccomplex = PyComplex_AsCComplex(value);

    // Write the datachar
    vd->bytes[vd->offset++] = COMPLEX_S;

    // Copy the real and imaginary parts to the bytes stack
    memcpy(&(vd->bytes[vd->offset]), &ccomplex.real, sizeof(double));
    memcpy(&(vd->bytes[vd->offset + sizeof(double)]), &ccomplex.imag, sizeof(double));
    vd->offset += 2 * sizeof(double);

    return SC_SUCCESS;
}

static inline StatusCode from_boolean(ValueData *vd, PyObject *value)
{
    if (!PyBool_Check(value)) return SC_INCORRECT;

    // Resize if necessary
    if (auto_resize_vd(vd, 1) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar based on whether the boolean is true
    vd->bytes[vd->offset++] = Py_IsTrue(value) ? BOOL_T : BOOL_F;

    return SC_SUCCESS;
}

static inline StatusCode from_bytes(ValueData *vd, PyObject *value)
{
    if (!PyBytes_Check(value)) return SC_INCORRECT;

    // Get the string as C bytes and get its size
    Py_ssize_t size;
    char *bytes;
    
    if (PyBytes_AsStringAndSize(value, &bytes, &size) == -1)
    {
        // Could not get the bytes' string and value
        PyErr_SetString(PyExc_RuntimeError, "Failed to get the C string representative of a bytes object.");
        return SC_EXCEPTION;
    }

    // Write the data
    if (write_E12D(vd, size, (const unsigned char *)bytes, BYTES_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Return success
    return SC_SUCCESS;
}

static inline StatusCode from_bytearray(ValueData *vd, PyObject *value)
{
    if (!PyByteArray_Check(value)) return SC_INCORRECT;

    // Get the string as C bytes
    const char *bytes = (const char *)PyByteArray_AsString(value);
    // Get the size of the bytearray object
    Py_ssize_t size = Py_SIZE(value);

    // Write the data
    if (write_E12D(vd, size, (const unsigned char *)bytes, BYTEARR_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Return success
    return SC_SUCCESS;
}

// Function for static values, like NoneType and Ellipsis
static inline StatusCode from_static_value(ValueData *vd, const unsigned char datachar)
{
    // Resize if necessary
    if (auto_resize_vd(vd, 1) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the received datachar
    vd->bytes[vd->offset++] = datachar;

    return SC_SUCCESS;
}

static inline StatusCode from_datetime(ValueData *vd, PyObject *value, const char *datatype) // Datetype required for the type of datetime object
{
    if (strcmp("datetime.timedelta", datatype) == 0)
    {
        if (!PyDelta_Check(value)) return SC_INCORRECT;

        // Resize if necessary
        if (auto_resize_vd(vd, 1 + (3 * sizeof(int))) == SC_NOMEMORY) return SC_NOMEMORY;

        // Write the datachar
        vd->bytes[vd->offset++] = DATETIME_TD;

        // Extract the components of the timedelta object
        int days = PyDateTime_DELTA_GET_DAYS(value);
        int seconds = PyDateTime_DELTA_GET_SECONDS(value);
        int microseconds = PyDateTime_DELTA_GET_MICROSECONDS(value);

        // Copy the components to the bytes stack
        memcpy(&(vd->bytes[vd->offset]), &days, sizeof(int));
        vd->offset += sizeof(int); // Increment to start at the unallocated space

        memcpy(&(vd->bytes[vd->offset]), &seconds, sizeof(int));
        vd->offset += sizeof(int);

        memcpy(&(vd->bytes[vd->offset]), &microseconds, sizeof(int));
        vd->offset += sizeof(int);
    }
    else
    {
        // Create an iso format string of the datetime object
        PyObject *iso = PyObject_CallMethod(value, "isoformat", NULL);
        if (iso == NULL) return SC_INCORRECT;
        
        // Convert the iso string to bytes
        Py_ssize_t size;
        const char *bytes = PyUnicode_AsUTF8AndSize(iso, &size);

        // Resize if necessary
        if (auto_resize_vd(vd, 2 + size) == SC_NOMEMORY) return SC_NOMEMORY;

        Py_DECREF(iso);

        // Decide the datachar of the datetime object type and do type checks
        if (strcmp("datetime.datetime", datatype) == 0)
        {
            if (!PyObject_IsInstance(value, datetime_dt)) return SC_INCORRECT;
            vd->bytes[vd->offset++] = DATETIME_DT;
        }
        else if (strcmp("datetime.date", datatype) == 0)
        {
            if (!PyObject_IsInstance(value, datetime_d)) return SC_INCORRECT;
            vd->bytes[vd->offset++] = DATETIME_D;
        }
        else if (strcmp("datetime.time", datatype) == 0)
        {
            if (!PyObject_IsInstance(value, datetime_t)) return SC_INCORRECT;
            vd->bytes[vd->offset++] = DATETIME_T;
        }
        else
            return SC_INCORRECT;
        
        // Write the size byte
        vd->bytes[vd->offset++] = (const unsigned char)size;
        // Write the bytes
        memcpy(&(vd->bytes[vd->offset]), bytes, size);
        vd->offset += size;
    }

    return SC_SUCCESS;
}

static inline StatusCode from_decimal(ValueData *vd, PyObject *value)
{
    if (!PyObject_IsInstance(value, decimal_cl)) return SC_INCORRECT;

    // Get the string representation of the Decimal object
    PyObject* str = PyObject_Str(value);
    if (str == NULL) return SC_INCORRECT;

    // Get the string as C bytes and get its size
    Py_ssize_t size;
    const char *bytes = PyUnicode_AsUTF8AndSize(str, &size);

    // Write the data, and pass the DECIMAL_1 datachar - 1 as there's not a DECIMAL_E datachar
    if (write_E12D(vd, size, (const unsigned char *)bytes, DECIMAL_1 - 1) == SC_NOMEMORY) return SC_NOMEMORY;

    // Return success
    return SC_SUCCESS;
}

static inline StatusCode from_uuid(ValueData *vd, PyObject *value)
{
    if (!PyObject_IsInstance(value, uuid_cl)) return SC_INCORRECT;

    // Resize if necessary
    if (auto_resize_vd(vd, 33) == SC_NOMEMORY) return SC_NOMEMORY;
    
    // Get the hexadecimal representation of the UUID
    PyObject* hex_str = PyObject_GetAttrString(value, "hex");
    if (hex_str == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not get the hex representation of a UUID.");
        return SC_EXCEPTION;
    }

    // Get the string as C bytes
    const char *bytes = PyUnicode_AsUTF8(hex_str);

    // Write the datachar
    vd->bytes[vd->offset++] = UUID_S;
    // Write the value itself
    memcpy(&(vd->bytes[vd->offset]), bytes, 32); // Static size of 32
    vd->offset += 32;

    // Return success
    return SC_SUCCESS;
}

static inline StatusCode from_memoryview(ValueData *vd, PyObject *value)
{
    if (!PyMemoryView_Check(value)) return SC_INCORRECT;

    // Get the underlying buffer from the memoryview
    Py_buffer view;
    if (PyObject_GetBuffer(value, &view, PyBUF_READ) == -1)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not get the buffer of a memoryview object.");
        return SC_EXCEPTION;
    }

    // Write the data
    if (write_E12D(vd, view.len, (const unsigned char *)view.buf, MEMVIEW_E) == SC_NOMEMORY) return SC_NOMEMORY;

    PyBuffer_Release(&view);

    return SC_SUCCESS;
}

// # Functions for converting list type values to bytes and their helper functions

// Pre-definition for the items in the iterables
static inline StatusCode from_any_value(ValueData *vd, PyObject *value);

static inline StatusCode from_list(ValueData *vd, PyObject *value)
{
    if (!PyList_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;

    // The number of items in the list
    Py_ssize_t num_items = PyList_Size(value);

    // Write the metadata, and pass NULL as the bytes to not write that
    if (write_E12D(vd, num_items, NULL, LIST_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Go over all items in the list
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the item from the list
        PyObject *item = PyList_GET_ITEM(value, i);
        // Write it to the valuedata and get the status
        int status = from_any_value(vd, item);

        // Return the status code if it's not success
        if (status != SC_SUCCESS) return status;
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

static inline StatusCode from_tuple(ValueData *vd, PyObject *value)
{
    if (!PyTuple_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;

    // The number of items in the tuple
    Py_ssize_t num_items = PyTuple_Size(value);

    // Write the metadata
    if (write_E12D(vd, num_items, NULL, TUPLE_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Go over all items in the tuple
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the item from the list
        PyObject *item = PyTuple_GetItem(value, i);
        // Write it to the valuedata and get the status
        int status = from_any_value(vd, item);

        // Return the status code if it's not success
        if (status != SC_SUCCESS) return status;
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

// Function fro both sets and frozensets
static inline StatusCode from_set_frozenset(ValueData *vd, PyObject *value, int is_frozenset)
{
    if (!PySet_Check(value) && !PyFrozenSet_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;
    
    // Create an iterator from the set so that we can count the items
    PyObject *count_iter = PyObject_GetIter(value);
    if (count_iter == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not get an iterator of a set type.");
        return SC_EXCEPTION;
    }
    
    // This will hold the number of items in the iterator
    Py_ssize_t num_items = 0;

    // Quickly go over the iterator to get the number of items
    while (PyIter_Next(count_iter) != NULL)
    {
        num_items++;
    }

    // Write the metadata
    if (is_frozenset == 1)
    {
        if (write_E12D(vd, num_items, NULL, FSET_E) == SC_NOMEMORY) return SC_NOMEMORY;
    }
    else
    {
        if (write_E12D(vd, num_items, NULL, SET_E) == SC_NOMEMORY) return SC_NOMEMORY;
    }

    // Get another iterator of the set to write the items
    PyObject *iter = PyObject_GetIter(value);
    if (iter == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not get an iterator of a set type.");
        return SC_EXCEPTION;
    }

    // Go over the iterator and write the items
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the item from the iterator
        PyObject *item = PyIter_Next(iter);
        // Write it to the valuedata and get the status
        int status = from_any_value(vd, item);

        Py_DECREF(item);

        // Return the status code if it's not success
        if (status != SC_SUCCESS) return status;
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

static inline StatusCode from_dict(ValueData *vd, PyObject *value)
{
    /*
      A dict type is treated kind of as a list, but the difference
      is that it uses dict datachars and places the items from the
      dict in the order of 'key1, value1, key2, value2, etc..'.

    */
    if (!PyDict_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;

    // Get the amount of item pairs in the dict
    Py_ssize_t num_pairs = PyDict_Size(value);

    // Write the metadata
    if (write_E12D(vd, num_pairs, NULL, DICT_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Get the items of the dict in a list
    PyObject *iterable = PyDict_Items(value);

    // Go over all items in the dict
    for (Py_ssize_t i = 0; i < num_pairs; i++)
    {
        // Get the pair of the items, which is a tuple with the key on 0 and value on 1
        PyObject *pair = PyList_GET_ITEM(iterable, i);

        // Get the key and item from the pair tuple
        PyObject *key = PyTuple_GET_ITEM(pair, 0);
        PyObject *item = PyTuple_GET_ITEM(pair, 1);

        // Write the key and item
        int status_key = from_any_value(vd, key);
        if (status_key != SC_SUCCESS) return status_key;
        
        int status_item = from_any_value(vd, item);
        if (status_item != SC_SUCCESS) return status_item;
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

// # The main from-value conversion functions

static inline StatusCode from_any_value(ValueData *vd, PyObject *value)
{
    // Get the datatype of the value
    const char *datatype = Py_TYPE(value)->tp_name;
    // Get the first character of the datatype
    const char datachar = *datatype;

    switch (datachar)
    {
    case 's': // String | Set
    {
        // Check the 2nd character
        switch (datatype[1])
        {
        case 't': return from_string(vd, value);
        case 'e': return from_set_frozenset(vd, value, 0);
        default:  return SC_INCORRECT;
        }
    }
    case 'i': return from_integer(vd, value);
    case 'f':
    {
        switch (datatype[1])
        {
        case 'l': return from_float(vd, value);
        case 'r': return from_set_frozenset(vd, value, 1);
        }
    }
    return from_float(vd, value);
    case 'c': return from_complex(vd, value);
    case 'b': // Boolean | bytes | bytearray (all start with a 'b')
    {
        // Check the 2nd datachar
        switch (datatype[1])
        {
        case 'o': return from_boolean(vd, value);
        default:
        {
            // Check the 5th datachar because the 2nd, 3rd, and 4th are the same
            switch (datatype[4])
            {
            case 's': return from_bytes(vd, value);
            case 'a': return from_bytearray(vd, value);
            default:  return SC_INCORRECT;
            }
        }
        }
    }
    case 'N': return from_static_value(vd, NONE_S);
    case 'e': return from_static_value(vd, ELLIPSIS_S);
    case 'd': // DateTime objects | Decimal | Dict
    {
        switch (datatype[1])
        {
        case 'a': return from_datetime(vd, value, datatype);
        case 'e': return from_decimal(vd, value);
        case 'i': return from_dict(vd, value);
        default:  return SC_INCORRECT;
        }
    }
    case 'U': return from_uuid(vd, value);
    case 'm': return from_memoryview(vd, value);
    case 'l': return from_list(vd, value);
    case 't': return from_tuple(vd, value);
    default:  return SC_INCORRECT;
    }
}

PyObject *from_value(PyObject *value)
{
    // Initiate the ValueData
    StatusCode vd_status;
    ValueData vd = init_vd(value, &vd_status);

    // Return on status error
    if (vd_status == SC_EXCEPTION)
        // Exception already set
        return NULL;

    // Write the value and get the status
    StatusCode status = from_any_value(&vd, value);

    // Check the status and throw an appropriate error if not success
    if (status == SC_SUCCESS)
    {
        // Convert it to a Python bytes object
        PyObject *py_bytes = PyBytes_FromStringAndSize((const char *)(vd.bytes), vd.offset);
        free(vd.bytes);
        return py_bytes;
    }
    else
    {
        free(vd.bytes);
        // Check what error we encountered
        switch (status)
        {
        case SC_INCORRECT:
        case SC_UNSUPPORTED:
        {
            // Incorrect datatype, likely an unsupported one
            PyErr_SetString(PyExc_ValueError, "Received an unsupported datatype.");
            return NULL;
        }
        case SC_EXCEPTION: return NULL; // Error already set
        case SC_NESTDEPTH:
        {
            // Exceeded the maximum nest depth
            PyErr_SetString(PyExc_ValueError, "Exceeded the maximum value nest depth.");
            return NULL;
        }
        case SC_NOMEMORY:
        {
            // Not enough memory
            PyErr_SetString(PyExc_MemoryError, "Not enough memory space available for use.");
            return NULL;
        }
        default:
        {
            // Something unknown went wrong
            PyErr_SetString(PyExc_RuntimeError, "Something unexpected went wrong, and we couldn't quite catch what it was.");
            return NULL;
        }
        }
    }
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
    PyObject *value = _PyLong_FromByteArray(&(bd->bytes[++bd->offset]), length, 1, 1);

    // Update the offset to start at the next item
    bd->offset += length;
    return value;
}

static inline PyObject *to_float_s(ByteData *bd)
{
    if (ensure_offset(bd, 1 + sizeof(double)) == -1) return NULL;

    // Get the bytes address that holds the double
    double *bytes = (double *)&(bd->bytes[++bd->offset]);

    // Get the string of the float
    PyObject *float_obj = PyFloat_FromDouble(*bytes);

    // Update the offset to start at the next item
    bd->offset += sizeof(double);

    return float_obj;
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

static inline PyObject *to_timedelta_s(ByteData *bd)
{
    // These will hold the days, seconds, and microseconds from the timedelta object
    int days, seconds, microseconds;

    memcpy(&days, &(bd->bytes[++bd->offset]), sizeof(int));
    bd->offset += sizeof(int); // Increment to start at the next chunk

    memcpy(&seconds, &(bd->bytes[bd->offset]), sizeof(int));
    bd->offset += sizeof(int);
    
    memcpy(&microseconds, &(bd->bytes[bd->offset]), sizeof(int));
    bd->offset += sizeof(int);

    // Create and return the timedelta object
    return PyDelta_FromDSU(days, seconds, microseconds);
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
    case DATETIME_TD: return to_timedelta_s(bd);
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
    const unsigned char *bytes = (const unsigned char *)PyBytes_AsString(py_bytes);

    // Get the first character, being the protocol marker
    const unsigned char protocol = *bytes;

    // Decide what to do based on the protocol version
    switch (protocol)
    {
    case PROT_STD_D: // The default STD protocol
    {
        // Get the length of this bytes object for memory allocation
        Py_ssize_t bytes_length = PyBytes_Size(py_bytes);

        // Create the bytedata struct
        ByteData *bd = (ByteData *)malloc(sizeof(ByteData));
        if (bd == NULL)
        {
            PyErr_SetString(PyExc_MemoryError, "No available memory space.");
            return NULL;
        }

        // Allocate the space for the bytes object
        bd->bytes = (unsigned char *)malloc((size_t)bytes_length);
        if (bd->bytes == NULL)
        {
            free(bd);
            PyErr_SetString(PyExc_MemoryError, "No available memory space.");
            return NULL;
        }

        // Write the bytes and offset to the bytedata
        memcpy((unsigned char *)(bd->bytes), bytes, (size_t)bytes_length);
        bd->offset = 1; // Start at offset 1 to exclude the prototype marker
        bd->max_offset = (size_t)bytes_length; // Set the max offset to the bytes length

        // Use and return the to-any-value conversion function
        PyObject *result = to_any_value(bd);
        free((void *)(bd->bytes));
        free(bd);
        
        if (result == NULL)
            return NULL;

        return result;
    }
    /*
      There aren't any other protocols currently. When there are, their
      de-serialization method is found in a stripped down version of the
      conversion file of that protocol, which is called 'to_value_protX'
      where 'X' is the protocol version. This will be used as follows:

      `case PROT_X: return to_value_protX(py_bytes);`

    */
    default: // Likely received an invalid bytes object
    {
        PyErr_Format(PyExc_ValueError, "Likely received an invalid bytes object: invalid protocol marker.");
        return NULL;
    }
    }
}

