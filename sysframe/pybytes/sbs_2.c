#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <datetime.h>
#include <ctype.h>

#include "sbs_old/sbs_1.h"
#include "sbs_2.h"

/*
  ## Explanation of the SBS (Structured Bytes Stack) protocol

  # What is a 'Structured Bytes Stack'?

  This protocol is called Structured Bytes Stack due to the method
  used for stacking items on top of each other. There is no global
  metadata dictionary anywhere, nor are there padding bytes in use.
  That's why it's 'Structured'. The exact length of the value is
  defined directly before it, and directly after the datatype
  representation character. Also, there are different structures
  in used to represent the exact length of the value. More on that
  later. An example of a serialized string:

    'DATACHAR + SIZE_BYTES + VALUE_BYTES'

  here, the datachar is that of a string (more on datachars below),
  the size bytes represent the size of the value bytes, and the
  value bytes is the string, but encoded (or, serialized).

  An example of how it would look with the string "hello":

    '0x01 0x05 h e l l o' (A space is placed between each character)

  Here, the '0x01' is the datatype representative for a string that
  requires less than 256 bytes to represent. The '0x05' is the length
  of the encoded string, and then of course, the encoded string itself.


  # The concept of datachars

  Datachar is just short for 'datatype character'. A datachar is an
  unique character for a specific datatype. These unique characters
  are written as numbers, starting from 0.


  # Datachar length representations

  Most datatypes are represented by multiple datachars. Their macro
  names are structured in a simple way: the datatype name, then an
  underscore, and then the length representation. For example, with
  a 'str' datatype, you can have STR_1.

  The length representations that follow the datatype name all have
  basically the same meaning. Usually, they're structured as follows:
  
    'E':  The value is empty. (not always present, because not always required).
    '1':  One byte is used to represent the byte length.
    '2':  Two bytes are used to represent the byte length.
    'D1': One byte is used to represent the length of the byte length representation.
    'D2': Same as D1, except the 'one byte' is multiple bytes, and the length of that is also represented in a byte before.
  
  Other than that, for static values, the tag 'S' is used. And if
  there are other tags or structures in use, they are explained on
  their macros explicitly.


  # Protocols

  There are multiple protocols in use. The older protocols are
  still supported for de-serialization, but no longer support
  the serialization. The standard serialization protocol that is
  currently in use is called `PROT_SBS_D`, which stands for
  'Protocol SBS, Default'. The 'SBS' tag is used to separate
  it from the SFS protocols.


  # Markers

  Markers are basically the miscellaneous datachars. They aren't
  used for representing datatypes and count downward from 255.
  The markers are explained at the macro definitions.

  Currently there's just one marker, and it isn't even in use yet.
  Though they might have a good use for later, so that's why.


  # SCs (Status Code)

  Within the serialization functions, we use SCs to mark errors or other
  issues. This is only in use due to the fact that serialization
  involves a broader range of potential issues, as well as common
  issues like receiving an unsupported or incorrect datatype.

  All issues can be handled in a single location this way, said
  location being the 'main' serialization function (from_value).
  There are exceptions that are used to set a static error message,
  but there's also one which allows you to set a custom one before
  returning it, that being 'SC_EXCEPTION'.


  # Plan for support

  The following datatypes are planned to be supported in the future:

  - collections.defaultdict
  - collections.ordereddict
  - pathlib.Path

  Aside those specific datatypes, more datatypes from core libraries
  will be included in the future as well, such as from 'collections'.

*/

// # 'Global' markers

#define EXT_M  255 // Reserved for if we ever happen to run out of a single byte to represent stuff
#define PROT_1 254 // Protocol 1
#define PROT_2 253 // Protocol 2

#define PROT_SBS_D PROT_2 // The default SBS protocol
#define PROT_SFS_D PROT_1 // The default SFS protocol

// # 'Standard' values

// String
#define STR_E  0
#define STR_1  1
#define STR_2  2
#define STR_D1 3
#define STR_D2 4

// Integer
#define INT_1   5 //* For integers, we don't use byte representations, as integers can be
#define INT_2   6 //  stored much more compact. Thus, INT_1 means the int value is 1 byte
#define INT_3   7 //  long, INT_2 means it's 2 bytes, etc., except for larger ints.
#define INT_4   8 //  
#define INT_5   9 //  The dynamic method for an int uses a single byte to represent the length
#define INT_D1 10 //  at the D1. At D2, we're just using the dynamic 2 method.
#define INT_D2 11 //* 

// Float
#define FLOAT_S 12

// Boolean
#define BOOL_T 13 // Use T for True values
#define BOOL_F 14 // Use F for False values

// Complex
#define COMPLEX_S 15

// NoneType
#define NONE_S 16

// Ellipsis
#define ELLIPSIS_S 17

// Bytes
#define BYTES_E  18
#define BYTES_1  19
#define BYTES_2  20
#define BYTES_D1 21
#define BYTES_D2 22

// ByteArray
#define BYTEARR_E  23
#define BYTEARR_1  24
#define BYTEARR_2  25
#define BYTEARR_D1 26
#define BYTEARR_D2 27

// # 'List type' values

// List
#define LIST_E  28
#define LIST_1  29
#define LIST_2  30
#define LIST_D1 31
#define LIST_D2 32

// Set
#define SET_E  33
#define SET_1  34
#define SET_2  35
#define SET_D1 36
#define SET_D2 37

// Tuple
#define TUPLE_E  38
#define TUPLE_1  39
#define TUPLE_2  40
#define TUPLE_D1 41
#define TUPLE_D2 42

// Dictionary
#define DICT_E  43
#define DICT_1  44
#define DICT_2  45
#define DICT_D1 46
#define DICT_D2 47

// FrozenSet
#define FSET_E  48
#define FSET_1  49
#define FSET_2  50
#define FSET_D1 51
#define FSET_D2 52

// # 'Miscellaneous' values (includes items you could consider 'list type', but placed here due to having to be imported)

// DateTime
#define DATETIME_DT 53 // DT for DateTime objects
#define DATETIME_TD 54 // TD for TimeDelta objects
#define DATETIME_D  55 // D  for Date objects
#define DATETIME_T  56 // T  for Time objects

// UUID
#define UUID_S 57

// MemoryView
#define MEMVIEW_E  58
#define MEMVIEW_1  59
#define MEMVIEW_2  60
#define MEMVIEW_D1 61
#define MEMVIEW_D2 62

// Decimal
#define DECIMAL_1  63
#define DECIMAL_2  64
#define DECIMAL_D1 65
#define DECIMAL_D2 66

// Range
#define RANGE_S 67

// Namedtuple
#define NTUPLE_E  68
#define NTUPLE_1  69
#define NTUPLE_2  70
#define NTUPLE_D1 71
#define NTUPLE_D2 72

// Deque
#define DEQUE_E  73
#define DEQUE_1  74
#define DEQUE_2  75
#define DEQUE_D1 76
#define DEQUE_D2 77

// Counter
#define COUNTER_E  78
#define COUNTER_1  79
#define COUNTER_2  80
#define COUNTER_D1 81
#define COUNTER_D2 82

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
#define MAX_NESTS  101 // The maximun amount of nests allowed, plus 1

// Datetime module classes
PyObject *datetime_dt; // datetime
PyObject *datetime_d;  // date
PyObject *datetime_t;  // time

// UUID module class
PyObject *uuid_cl;

// Decimal module class
PyObject *decimal_cl;

// Namedtuple module class
PyObject *namedtuple_cl;

// Deque module class
PyObject *deque_cl;

// Counter module class
PyObject *counter_cl;

// # Initialization and cleanup functions

int sbs2_init(void)
{
    /*
      As this module supports various datatypes from core libraries, we
      have to import those. For performance, the specific classes of said
      modules that we have to use for the serialization methods are defined,
      so that we don't have to get those attributes at runtime.

    */

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
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'datetime' in module 'datetime'.");
        return -1;
    }
    else if (datetime_d == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'date' in module 'datetime'.");
        return -1;
    }
    else if (datetime_t == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'time' in module 'datetime'.");
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
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'UUID' in module 'uuid'.");
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

    // Get the collections module
    PyObject *collections_m = PyImport_ImportModule("collections");
    if (collections_m == NULL)
    {
        PyErr_SetString(PyExc_ModuleNotFoundError, "Could not find module 'collections'.");
        return -1;
    }

    // Get the required attributes
    namedtuple_cl = PyObject_GetAttrString(collections_m, "namedtuple");
    deque_cl = PyObject_GetAttrString(collections_m, "deque");
    counter_cl = PyObject_GetAttrString(collections_m, "Counter");

    if (namedtuple_cl == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'namedtuple' in module 'collections'.");
        return -1;
    }
    else if (deque_cl == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'deque' in module 'collections'.");
        return -1;
    }
    else if (counter_cl == NULL)
    {
        PyErr_SetString(PyExc_AttributeError, "Could not find attribute 'Counter' in module 'collections'.");
        return -1;
    }

    return 1;
}

void sbs2_cleanup(void)
{
    // Decref all modules with XDECREF as they're NULL when not found
    Py_XDECREF(datetime_dt);
    Py_XDECREF(datetime_d);
    Py_XDECREF(datetime_t);
    Py_XDECREF(uuid_cl);
    Py_XDECREF(decimal_cl);
    Py_XDECREF(namedtuple_cl);
    Py_XDECREF(deque_cl);
    Py_XDECREF(counter_cl);
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
    /*
      This function automatically resizes the allocated space for the bytes
      variable in the ValueData struct. It returns a status code just like
      the datatype serialization functions to handle failed reallocs.

    */

    // Check if we need to reallocate for more space with the given jump
    if (vd->offset + jump > vd->max_size)
    {
        // Update the max size
        vd->max_size += jump + ALLOC_SIZE;
        // Reallocate to the new max size
        unsigned char *temp = (unsigned char *)realloc((void *)(vd->bytes), vd->max_size * sizeof(unsigned char));
        if (temp == NULL)
        {
            // Free the already allocated bytes
            free(vd->bytes);
            return SC_NOMEMORY;
        }

        // Update the bytes to point to the new allocated bytes
        vd->bytes = temp;
    }

    // Return success
    return SC_SUCCESS;
}

// This function updates the nest depth and whether we've reached the max nests
static inline StatusCode increment_nests(ValueData *vd)
{
    /*
      This function keeps track of the current nest depth, to ensure we
      don't go past the max nests. Technically, we could support an arbitrary
      depth, but this is used to handle circular references. Thus, a large nest
      depth is supported to maintain versatility.

    */

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
    /*
      This function creates a ValueData struct for us, and pre-allocates
      an estimated size of the value plus the default alloc size to have
      enough headroom, while still refraining from allocating either too
      little, causing performance degradation due to frequent reallocs,
      or too much, using up unnecessary memory space.

    */

    // Attempt to estimate what the max possible byte size will be using sys.getsizeof
    size_t max_size = _PySys_GetSizeOf(value) + ALLOC_SIZE; // Add the alloc size as headroom

    // Create the struct itself
    ValueData vd = {1, (Py_ssize_t)max_size, 0, (unsigned char *)malloc(max_size * sizeof(unsigned char))};
    if (vd.bytes == NULL)
    {
        // Set the status
        *status = SC_NOMEMORY;
        return vd;
    }

    // Write the protocol byte
    const unsigned char protocol = PROT_SBS_D;
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

// Function to write size bytes
static inline void write_size_bytes(ValueData *vd, Py_ssize_t num, Py_ssize_t num_bytes)
{
    /*
      Write a number as bytes to the bytes stack. This is done by
      taking the first 256 bytes and storing them as a single char,
      then shifting them to the next 256 bytes and store those, etc.

    */

    // Write the size as bytes
    for (Py_ssize_t i = 0; i < num_bytes; i++) {
        vd->bytes[vd->offset++] = (unsigned char)(num & 0xFF);
        num >>= 8;
    }
}

// Function to write the datachar and the size as bytes
static inline StatusCode write_metadata(ValueData *vd, const unsigned char datachar, Py_ssize_t num, Py_ssize_t num_bytes)
{
    /*
      This function stores the 'regular' (non-dynamic method) metadata
      for a value. The metadata is just the datachar with the size bytes
      written after it.

    */

    // Write the datachar
    vd->bytes[vd->offset++] = datachar;

    // Write the size bytes
    write_size_bytes(vd, num, num_bytes);

    return SC_SUCCESS;
}

// Alike the regular write-metadata function, but also writes the dynamic datachar
static inline StatusCode write_dynamic1_metadata(ValueData *vd, const unsigned char datachar, Py_ssize_t num, Py_ssize_t num_bytes)
{
    /*
      This function writes the metadata for a dynamic 1 method.
      The dynamic 1 method stores the size of the value by writing
      the length of the size bytes as one byte, and then the size
      bytes, so that we know the exact length of the size bytes.

      If the length of the size bytes can't be stored in a single byte,
      the dynamic 2 method is used.

    */

    // Resize if necessary
    if (auto_resize_vd(vd, num_bytes + 2) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
    vd->bytes[vd->offset++] = datachar;
    // Write the dynamic size byte
    vd->bytes[vd->offset++] = (const unsigned char)num_bytes;

    // Write the size bytes
    write_size_bytes(vd, num, num_bytes);

    return SC_SUCCESS;
}

static inline StatusCode write_dynamic2_metadata(ValueData *vd, const unsigned char datachar, Py_ssize_t num, Py_ssize_t num_bytes)
{
    /*
      This function writes the metadata for a dynamic 2 method.
      This is similar to the dynamic 1 method, except it supports
      even larger byte sizes by storing the length of the length of
      the size bytes as well. Because that will fit in 1 byte for sure.

      The phrase 'the length of the length of the size bytes' might
      sound a bit confusing, so here's what it means exactly: As the
      length of the size bytes will be more than 1 byte, we have to know
      how many bytes it will take up. So, that number is 'the length of
      the length of the size bytes'.

      The limit of supporting up to 255^255^255-1 bytes (because this
      method allows that) is practically unreachable. Thus, this is the
      limit we have. If sometime in the future we will ever need to support
      a size larger than this, I'm not sure if this module will even be
      relevant anymore. So no need to worry about that limit, it's plenty.

    */
    
    // Get the length of the num bytes
    Py_ssize_t num_bytes_length = get_num_bytes(num_bytes);

    // Resize if necessary
    if (auto_resize_vd(vd, num_bytes + 2) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
    vd->bytes[vd->offset++] = datachar;
    // Write the num bytes length
    vd->bytes[vd->offset++] = (const unsigned char)num_bytes_length;
    // Write the num bytes
    write_size_bytes(vd, num_bytes, num_bytes_length);

    // Write the size bytes
    write_size_bytes(vd, num, num_bytes);

    return SC_SUCCESS;
}

// Function to write the full data with an E-1-2-D setup
static inline StatusCode write_E12D(ValueData *vd, Py_ssize_t size, const unsigned char *bytes, const unsigned char empty)
{
    /*
      This function is used to store the full data of a datatype, which
      is just the metadata plus the bytes of the value. The 'E-1-2-D' setup
      is just the setup where there's an optional 'E' tag with the datachars,
      and a mandatory '1', '2', 'D1', and 'D2' tag. This function calculates
      which to use and writes the size bytes for us.

    */

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
        write_metadata(vd, empty + num_bytes, size, num_bytes);
        break;
    }
    default:
    {
        // Check whether to use dynamic 1 or 2 method
        if (num_bytes < 256) // Smaller than 1 byte
        {
            // Resize if necessary
            if (auto_resize_vd(vd, num_bytes + size + 1) == SC_NOMEMORY) return SC_NOMEMORY;
            // Write the dynamic metadata
            if (write_dynamic1_metadata(vd, (const unsigned char)(empty + 3), size, num_bytes) == SC_NOMEMORY) return SC_NOMEMORY;
        }
        else if (num_bytes < (255^255) - 1) // Smaller than 256 bytes
        {
            // Resize if necessary
            if (auto_resize_vd(vd, num_bytes + size + 1) == SC_NOMEMORY) return SC_NOMEMORY;
            // Write the dynamic metadata
            if (write_dynamic2_metadata(vd, empty + 4, size, num_bytes) == SC_NOMEMORY) return SC_NOMEMORY;
        }
        else
        {
            // We don't support values of this size, as this is highly unlikely to even be reached
            PyErr_SetString(PyExc_ValueError, "Values of this size aren't supported.");
            return SC_EXCEPTION;
        }
        
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
    /*
      This function converts a C bytes object to the number it represents,
      with little-endianness. This is done by moving the byte of each
      iteration to the start, creating a number of that one byte. And that
      number gets shifted onto the 'total' number, creating the actual number
      after the amount of parsed iterations, or 'length'.

    */
    
    size_t num = 0;

    // Convert the byte array back to a size_t value
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
    /*
      As integers are stored slightly differently compared to the
      other datatypes, we will handle them more directly here compared
      to using the 'write_E12D' function. This is also why the integer
      has more datachars, so that we can store it slightly more compact.

    */
    
    if (!PyLong_Check(value)) return SC_INCORRECT;

    // Calculate number of bytes needed, including the sign bit
    size_t num_bytes = Py_SIZE(value) > 0 ? (_PyLong_NumBits(value) + 8) / 8 : 1;

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
        // Get the length of the num bytes
        size_t num_bytes_length = get_num_bytes(num_bytes);

        // Check whether the num bytes length doesn't exceed the limit
        if (num_bytes_length > 255)
        {
            PyErr_SetString(PyExc_ValueError, "Integers of this size are not supported.");
            return SC_EXCEPTION;
        }

        // Resize if necessary
        if (auto_resize_vd(vd, 1 + num_bytes_length) == SC_NOMEMORY) return SC_NOMEMORY;

        // Write the num bytes length
        vd->bytes[vd->offset++] = (const unsigned char)num_bytes_length;

        // Write the num bytes themselves
        write_size_bytes(vd, num_bytes, num_bytes_length);
    }

    // Write the bytes directly to the bytes stack
    if (_PyLong_AsByteArray((PyLongObject *)value, &(vd->bytes[vd->offset]), num_bytes, 1, 1) == -1) return SC_EXCEPTION;

    // Update the offset
    vd->offset += num_bytes;

    return SC_SUCCESS;
}

static inline StatusCode from_float(ValueData *vd, PyObject *value)
{
    if (!PyFloat_Check(value)) return SC_INCORRECT;

    // Resize if necessary
    if (auto_resize_vd(vd, 1 + sizeof(double)) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
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

    // Resize if necessary
    if (auto_resize_vd(vd, 1 + view.len) == SC_NOMEMORY)
    {
        PyBuffer_Release(&view);
        return SC_NOMEMORY;
    }

    // Write the data
    if (write_E12D(vd, view.len, (const unsigned char *)view.buf, MEMVIEW_E) == SC_NOMEMORY) return SC_NOMEMORY;

    PyBuffer_Release(&view);

    return SC_SUCCESS;
}

static inline StatusCode from_range(ValueData *vd, PyObject *value)
{
    // As the range attributes can have arbitrary size, use the from_integer function for them

    // Resize if necessary
    if (auto_resize_vd(vd, 1) == SC_NOMEMORY) return SC_NOMEMORY;

    // Write the datachar
    vd->bytes[vd->offset++] = RANGE_S;

    // Get the start, stop, and step attributes
    PyObject *start = PyObject_GetAttrString(value, "start");
    PyObject *stop = PyObject_GetAttrString(value, "stop");
    PyObject *step = PyObject_GetAttrString(value, "step");

    // Write them as integers
    StatusCode status; // This will hold the status
    if ((status = from_integer(vd, start)) != SC_SUCCESS) return status; // Set the status and return it if not success
    if ((status = from_integer(vd, stop)) != SC_SUCCESS) return status;
    if ((status = from_integer(vd, step)) != SC_SUCCESS) return status;

    // Decref the attributes as we created them ourselves
    Py_DECREF(start);
    Py_DECREF(stop);
    Py_DECREF(step);

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
        // Write it to the bytes stack
        StatusCode status; // This will hold the status
        if ((status = from_any_value(vd, item)) != SC_SUCCESS) return status; // Set the status and return it if not success
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

static inline StatusCode from_tuple(ValueData *vd, PyObject *value)
{
    // Already checked if it's a tuple
    
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
        PyObject *item = PyTuple_GET_ITEM(value, i);
        // Write it to the bytes stack
        StatusCode status;
        if ((status = from_any_value(vd, item)) != SC_SUCCESS) return status;

        // Return the status code if it's not success
        if (status != SC_SUCCESS) return status;
    }

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

// Function for any iterable type value
static inline StatusCode from_iterable(ValueData *vd, PyObject *value, const unsigned char empty) // Get the E-tag datachar for the 'write_E12D' function
{
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;
    
    // Create an iterator from the set so that we can count the items
    PyObject *count_iter = PyObject_GetIter(value);
    if (count_iter == NULL) return SC_INCORRECT; // Likely not an iterable type that we received
    
    // This will hold the number of items in the iterator
    Py_ssize_t num_items = 0;

    // Quickly go over the iterator to get the number of items
    while (PyIter_Next(count_iter) != NULL)
    {
        num_items++;
    }

    Py_DECREF(count_iter);

    // Write the metadata
    if (write_E12D(vd, num_items, NULL, empty) == SC_NOMEMORY) return SC_NOMEMORY;

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
        StatusCode status = from_any_value(vd, item);
        Py_DECREF(item);

        // Return the status code if it's not success
        if (status != SC_SUCCESS) return status;
    }

    Py_DECREF(iter);

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

static inline StatusCode from_namedtuple(ValueData *vd, PyObject *value)
{
    // Get the fields of the namedtuple
    PyObject *fields = PyObject_GetAttrString(value, "_fields");

    // Get the number of fields
    Py_ssize_t num_items = PyTuple_Size(fields);

    // Write the metadata
    if (write_E12D(vd, num_items, NULL, NTUPLE_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Get the name of the module
    PyObject *name = PyObject_GetAttrString((PyObject *)Py_TYPE(value), "__name__");

    // Write the name of the module
    StatusCode status;
    if ((status = from_string(vd, name)) != SC_SUCCESS) return status;
    Py_DECREF(name);

    // Iterate over the fields
    for (Py_ssize_t i = 0; i < num_items; i++)
    {
        // Get the field name
        PyObject *name = PyTuple_GET_ITEM(fields, i);
        // Get the field item
        PyObject *item = PyTuple_GET_ITEM(value, i);

        // Write them to the bytes stack
        if ((status = from_string(vd, name)) != SC_SUCCESS) return status;
        if ((status = from_any_value(vd, item)) != SC_SUCCESS) return status;
    }

    return SC_SUCCESS;
}

static inline StatusCode from_dict(ValueData *vd, PyObject *value)
{
    if (!PyDict_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;

    // Get the amount of item pairs in the dict
    Py_ssize_t num_pairs = PyDict_Size(value);

    // Write the metadata
    if (write_E12D(vd, num_pairs, NULL, DICT_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Get the items of the dict in a list
    PyObject *iter = PyDict_Items(value);

    // Go over all items in the dict
    for (Py_ssize_t i = 0; i < num_pairs; i++)
    {
        // Get the pair of the items, which is a tuple with the key on 0 and value on 1
        PyObject *pair = PyList_GET_ITEM(iter, i);

        // Get the key and item from the pair tuple
        PyObject *key = PyTuple_GET_ITEM(pair, 0);
        PyObject *val = PyTuple_GET_ITEM(pair, 1);

        // Write the key and item
        StatusCode status;
        if ((status = from_any_value(vd, key)) != SC_SUCCESS) return status;
        if ((status = from_any_value(vd, val)) != SC_SUCCESS) return status;
    }

    Py_DECREF(iter);

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

static inline StatusCode from_counter(ValueData *vd, PyObject *value)
{
    if (!PyDict_Check(value)) return SC_INCORRECT;
    
    // Increment the nest depth and return if it's too deep
    if (increment_nests(vd) == SC_NESTDEPTH) return SC_NESTDEPTH;

    // Get the amount of item pairs in the dict
    Py_ssize_t num_pairs = PyDict_Size(value);

    // Write the metadata
    if (write_E12D(vd, num_pairs, NULL, COUNTER_E) == SC_NOMEMORY) return SC_NOMEMORY;

    // Get the items of the dict in a list
    PyObject *iter = PyDict_Items(value);

    // Go over all items in the dict
    for (Py_ssize_t i = 0; i < num_pairs; i++)
    {
        // Get the items pair
        PyObject *pair = PyList_GET_ITEM(iter, i);

        // Get the key and value from the pair tuple
        PyObject *key = PyTuple_GET_ITEM(pair, 0);
        PyObject *val = PyTuple_GET_ITEM(pair, 1);

        // Write the key and item
        StatusCode status;
        if ((status = from_any_value(vd, key)) != SC_SUCCESS) return status;
        if ((status = from_integer(vd, val)) != SC_SUCCESS) return status; // The value can only be an integer in a counter
    }

    Py_DECREF(iter);

    // Decrement the nest depth
    vd->nests--;

    return SC_SUCCESS;
}

// # The main from-value conversion functions

static inline StatusCode from_any_value(ValueData *vd, PyObject *value)
{
    // Check for special types that stand under tuples and types
    if (PyTuple_Check(value))
    {
        // Check if it's an actual tuple
        if (PyTuple_CheckExact(value))
            return from_tuple(vd, value);
        
        // Check if it has the fields attribute of a namedtuple
        else if (PyObject_HasAttrString(value, "_fields"))
            return from_namedtuple(vd, value);
        
        // Unsupported tuple type
        else return SC_UNSUPPORTED;
    }
    else if (PyType_Check(value))
    {
        // Types are not supported, but might be later
        return SC_UNSUPPORTED;
    }
    else
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
            case 'e': return from_iterable(vd, value, SET_E);
            default:  return SC_INCORRECT;
            }
        }
        case 'i': return from_integer(vd, value);
        case 'f':
        {
            switch (datatype[1])
            {
            case 'l': return from_float(vd, value);
            case 'r': return from_iterable(vd, value, FSET_E);
            }
        }
        return from_float(vd, value);
        case 'c': // Complex | Collections types
        {
            // Check if the datatype starts with 'collections'
            if (strncmp(datatype, "collections.", strlen("collections.")) == 0)
            {
                // It's an item from the collections module. Switch over the first character that comes after the "collections." (idx 12)
                const char new_datachar = datatype[12];
                switch (new_datachar)
                {
                case 'd': return from_iterable(vd, value, DEQUE_E);
                default:  return SC_UNSUPPORTED;
                }
            }
            else return from_complex(vd, value);
        }
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
        case 'r': return from_range(vd, value);
        case 'C': return from_counter(vd, value);
        default:  return SC_UNSUPPORTED;
        }
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
            break;
        }
        case SC_NESTDEPTH:
        {
            // Exceeded the maximum nest depth
            PyErr_SetString(PyExc_ValueError, "Exceeded the maximum value nest depth.");
            break;
        }
        case SC_NOMEMORY:
        {
            // Not enough memory
            PyErr_SetString(PyExc_MemoryError, "Not enough memory space available for use.");
            break;
        }
        case SC_EXCEPTION: break; // Error message is set by the returner
        default:
        {
            // Something unknown went wrong
            PyErr_SetString(PyExc_RuntimeError, "Something unexpected went wrong, and we couldn't quite catch what it was.");
            break;
        }
        }

        return NULL;
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

// Function for getting the size byte length of the dynamic 1 method
static inline size_t D1_length(ByteData *bd)
{
    // Ensure offset for the first size byte
    if (ensure_offset(bd, 1) == -1) return 0;

    // Get the length of the length bytes from the 1st character away from the offset
    size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
    return size_bytes_length;
}

// Function for getting the size byte length of the dynamic 2 method
static inline size_t D2_length(ByteData *bd)
{
    // Ensure offset for the first size byte
    if (ensure_offset(bd, 1) == -1) return 0;

    // Get the length of the length of the length bytes (sounds complicated, but explained in 'write_E12D' function)
    size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);

    // Ensure the offset for the 2nd size bytes
    if (ensure_offset(bd, length) == -1) return 0;

    // Get the length of the length bytes
    size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), length);

    // Update the offset to start at the value bytes
    bd->offset += length - 1;

    return size_bytes_length;
}

// # The to-conversion functions

// Pre-definition of the global conversion function to use it in to-conversion functions
static inline PyObject *to_any_value(ByteData *bd);

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

static inline PyObject *to_range_s(ByteData *bd)
{
    // Increment the offset by 1 for the datachar
    bd->offset++;

    // Get the Python integers that represent the start, stop, and step
    PyObject *start = to_any_value(bd);
    PyObject *stop  = to_any_value(bd);
    PyObject *step  = to_any_value(bd);

    // Create a range object with the attributes by calling the range class
    PyObject *range = PyObject_CallFunction((PyObject *)&PyRange_Type, "OOO", start, stop, step);

    // Decref the numbers we created as they're no longer necessary
    Py_DECREF(start);
    Py_DECREF(stop);
    Py_DECREF(step);

    return range;
}

// # The list type conversion functions and their helper functions

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

// This function is used for the values also converted with the 'from_iterable' function
static inline PyObject *to_iterable_e(ByteData *bd, const unsigned char empty) // Use the empty datachar to get the datatype
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    bd->offset++;

    // Create an empty list to create the set out of
    PyObject *empty_list = PyList_New(0);

    // This will hold the iterable type
    PyObject *iter;

    // Switch over the empty datachar to get which datatype it is
    switch (empty)
    {
    case SET_E:
    {
        iter = PySet_New(empty_list);
        break;
    }
    case FSET_E:
    {
        iter = PyFrozenSet_New(empty_list);
        break;
    }
    case DEQUE_E:
    {
        iter = PyObject_CallFunction(deque_cl, "O", empty_list);
        break;
    }
    default: // Shouldn't be reached, but just to be sure
    {
        PyErr_SetString(PyExc_RuntimeError, "Unexpectedly received an invalid iterable character.");
        return NULL;
    }
    }

    Py_DECREF(empty_list);
    return iter;
}

// Same as the function above but for non-empty iterables
static inline PyObject *to_iterable_gen(ByteData *bd, size_t size_bytes_length, const unsigned char empty)
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
    
    // This will hold the iterable type
    PyObject *iter;

    // Switch over the empty datachar to get which datatype it is
    switch (empty)
    {
    case SET_E:
    {
        iter = PySet_New(list);
        break;
    }
    case FSET_E:
    {
        iter = PyFrozenSet_New(list);
        break;
    }
    case DEQUE_E:
    {
        iter = PyObject_CallFunction(deque_cl, "O", list);
        break;
    }
    default: // Shouldn't be reached, but just to be sure
    {
        PyErr_SetString(PyExc_RuntimeError, "Unexpectedly received an invalid iterable character.");
        return NULL;
    }
    }

    Py_DECREF(list);
    return iter;
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

    // Create an empty dict object
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

        Py_DECREF(key);
        Py_DECREF(value);
    }

    return dict;
}

static inline PyObject *to_counter_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;
    bd->offset++;

    // Create and return a new counter
    return PyObject_CallObject(counter_cl, NULL);
}

static inline PyObject *to_counter_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the number of pairs in the dict
    size_t num_items = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    // Create an empty dict
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

        Py_DECREF(key);
        Py_DECREF(value);
    }

    // Create the counter out of the dict
    PyObject *counter = PyObject_CallFunctionObjArgs(counter_cl, dict, NULL);
    Py_DECREF(dict);

    return counter;
}

static inline PyObject *to_namedtuple_e(ByteData *bd)
{
    if (ensure_offset(bd, 1) == -1) return NULL;

    // Increment over the datachar
    bd->offset++;

    // Get the name of the namedtuple
    PyObject *name = to_any_value(bd);
    if (name == NULL) return NULL; // Error already set

    // Create an empty tuple to initialize the empty namedtuple with
    PyObject *empty_tuple = PyTuple_New(0);

    // Create the namedtuple type
    PyObject *nt_type = PyObject_CallFunction(namedtuple_cl, "OO", name, empty_tuple);
    // Create the namedtuple using that type
    PyObject *namedtuple = PyObject_CallObject(nt_type, empty_tuple);

    Py_DECREF(name);
    Py_DECREF(empty_tuple);
    Py_DECREF(nt_type);

    return namedtuple;
}

static inline PyObject *to_namedtuple_gen(ByteData *bd, size_t size_bytes_length)
{
    if (ensure_offset(bd, size_bytes_length + 1) == -1) return NULL;

    // Get the number of items
    size_t num_items = bytes_to_size_t(&(bd->bytes[++bd->offset]), size_bytes_length);
    bd->offset += size_bytes_length;

    // Get the name of the namedtuple
    PyObject *name = to_any_value(bd);
    if (name == NULL) return NULL; // Error already set

    // Go over the pairs and get the fields and items
    PyObject *fields = PyTuple_New(num_items);
    PyObject *items = PyTuple_New(num_items);
    for (Py_ssize_t i = 0; i < (Py_ssize_t)num_items; i++)
    {
        // Get the field and item
        PyObject *field = to_any_value(bd);
        PyObject *item = to_any_value(bd);

        if (field == NULL || item == NULL)
        {
            Py_XDECREF(field);
            Py_XDECREF(item);
            // Error already set
            return NULL;
        }

        // Set the field and item
        PyTuple_SetItem(fields, i, field);
        PyTuple_SetItem(items, i, item);
    }

    // Create the namedtuple type
    PyObject *nt_type = PyObject_CallFunctionObjArgs(namedtuple_cl, name, fields, NULL);
    // Create the namedtuple using that type
    PyObject *namedtuple = PyObject_CallObject(nt_type, items);

    Py_DECREF(name);
    Py_DECREF(fields);
    Py_DECREF(items);
    Py_DECREF(nt_type);

    return namedtuple;
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
    case STR_D1:
    {
        // Get the size bytes length for the dynamic 1 method
        size_t size_bytes_length = D1_length(bd);
        // Return NULL if the length is zero, indicating failure
        if (size_bytes_length == 0) return NULL;
        // Call the generic function with the size bytes length
        return to_str_gen(bd, size_bytes_length);
    }
    case STR_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_str_gen(bd, size_bytes_length);
    }
    case INT_1: return to_int_gen(bd, 1);
    case INT_2: return to_int_gen(bd, 2);
    case INT_3: return to_int_gen(bd, 3);
    case INT_4: return to_int_gen(bd, 4);
    case INT_5: return to_int_gen(bd, 5);
    case INT_D1:
    {
        // The integers dynamic 1 method is a bit different with a single byte size, so do it directly

        // Ensure the offset for the size byte
        if (ensure_offset(bd, 1) == -1) return NULL;
        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic to_int method
        return to_int_gen(bd, length);
    }
    case INT_D2:
    {
        // The size bytes length can be received using the dynamic 2 function, so use that
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_int_gen(bd, size_bytes_length);
    }
    case FLOAT_S:    return to_float_s(bd);
    case BOOL_T:     return to_bool_gen(bd, Py_True);
    case BOOL_F:     return to_bool_gen(bd, Py_False);
    case COMPLEX_S:  return to_complex_s(bd);
    case NONE_S:     return to_none_s(bd);
    case ELLIPSIS_S: return to_ellipsis_s(bd);
    case BYTES_E:    return to_bytes_e(bd, 0);
    case BYTES_1:    return to_bytes_gen(bd, 1, 0);
    case BYTES_2:    return to_bytes_gen(bd, 2, 0);
    case BYTES_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_bytes_gen(bd, size_bytes_length, 0);
    }
    case BYTES_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_bytes_gen(bd, size_bytes_length, 0);
    }
    case BYTEARR_E: return to_bytes_e(bd, 1);
    case BYTEARR_1: return to_bytes_gen(bd, 1, 1);
    case BYTEARR_2: return to_bytes_gen(bd, 2, 1);
    case BYTEARR_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_bytes_gen(bd, size_bytes_length, 1);
    }
    case BYTEARR_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
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
    case MEMVIEW_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_memoryview_gen(bd, size_bytes_length);
    }
    case MEMVIEW_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_memoryview_gen(bd, size_bytes_length);
    }
    case DECIMAL_1: return to_decimal_gen(bd, 1);
    case DECIMAL_2: return to_decimal_gen(bd, 2);
    case DECIMAL_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_decimal_gen(bd, size_bytes_length);
    }
    case DECIMAL_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_decimal_gen(bd, size_bytes_length);
    }
    case LIST_E: return to_list_e(bd);
    case LIST_1: return to_list_gen(bd, 1);
    case LIST_2: return to_list_gen(bd, 2);
    case LIST_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_list_gen(bd, size_bytes_length);
    }
    case LIST_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_list_gen(bd, size_bytes_length);
    }
    case TUPLE_E: return to_tuple_e(bd);
    case TUPLE_1: return to_tuple_gen(bd, 1);
    case TUPLE_2: return to_tuple_gen(bd, 2);
    case TUPLE_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_tuple_gen(bd, size_bytes_length);
    }
    case TUPLE_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_tuple_gen(bd, size_bytes_length);
    }
    case SET_E: return to_iterable_e(bd, SET_E);
    case SET_1: return to_iterable_gen(bd, 1, SET_E);
    case SET_2: return to_iterable_gen(bd, 2, SET_E);
    case SET_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, SET_E);
    }
    case SET_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, SET_E);
    }
    case FSET_E: return to_iterable_e(bd, FSET_E);
    case FSET_1: return to_iterable_gen(bd, 1, FSET_E);
    case FSET_2: return to_iterable_gen(bd, 2, FSET_E);
    case FSET_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, FSET_E);
    }
    case FSET_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, FSET_E);
    }
    case DICT_E: return to_dict_e(bd);
    case DICT_1: return to_dict_gen(bd, 1);
    case DICT_2: return to_dict_gen(bd, 2);
    case DICT_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_dict_gen(bd, size_bytes_length);
    }
    case DICT_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_dict_gen(bd, size_bytes_length);
    }
    case RANGE_S: return to_range_s(bd);
    case NTUPLE_E: return to_namedtuple_e(bd);
    case NTUPLE_1: return to_namedtuple_gen(bd, 1);
    case NTUPLE_2: return to_namedtuple_gen(bd, 2);
    case NTUPLE_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_namedtuple_gen(bd, size_bytes_length);
    }
    case NTUPLE_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_namedtuple_gen(bd, size_bytes_length);
    }
    case DEQUE_E: return to_iterable_e(bd, DEQUE_E);
    case DEQUE_1: return to_iterable_gen(bd, 1, DEQUE_E);
    case DEQUE_2: return to_iterable_gen(bd, 2, DEQUE_E);
    case DEQUE_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, DEQUE_E);
    }
    case DEQUE_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_iterable_gen(bd, size_bytes_length, DEQUE_E);
    }
    case COUNTER_E: return to_counter_e(bd);
    case COUNTER_1: return to_counter_gen(bd, 1);
    case COUNTER_2: return to_counter_gen(bd, 2);
    case COUNTER_D1:
    {
        size_t size_bytes_length = D1_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_counter_gen(bd, size_bytes_length);
    }
    case COUNTER_D2:
    {
        size_t size_bytes_length = D2_length(bd);
        if (size_bytes_length == 0) return NULL;
        return to_counter_gen(bd, size_bytes_length);
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
    case PROT_SBS_D: // The default SBS protocol
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

        return result;
    }
    case PROT_1: return to_value_prot1(py_bytes);
    default: // Likely received an invalid bytes object
    {
        PyErr_Format(PyExc_ValueError, "Likely received an invalid bytes object: invalid protocol marker.");
        return NULL;
    }
    }
}

