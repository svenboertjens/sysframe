#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <datetime.h>
#include <ctype.h>

// Include the current SBS protocol as it offsets pre-imported values we need
#include "sbs_2.h"

/*
  This is the SBS protocol 1 file, stripped down for just to-conversions.
  This file should not receive changes, as it's from an old protocol.
  
*/

// # 'Standard' values

// String
#define STR_E 0
#define STR_1 1
#define STR_2 2
#define STR_D 3

// Integer
#define INT_1   4
#define INT_2   5
#define INT_3   6
#define INT_4   7
#define INT_5   8
#define INT_D1  9
#define INT_D2 10

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
#define DATETIME_TD 46 // TD for TimeDelta objects
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

// Init function
void sbs1_init(void)
{
    // We only have to import this one ourselves
    PyDateTime_IMPORT;
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
        size_t length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 2);
        // Increment once more to start at the int value bytes
        bd->offset++;
        // Use and return the generic to_int method
        return to_int_gen(bd, length);
    }
    case INT_1:      return to_int_gen(bd, 1);
    case INT_2:      return to_int_gen(bd, 2);
    case INT_3:      return to_int_gen(bd, 3);
    case INT_4:      return to_int_gen(bd, 4);
    case INT_5:      return to_int_gen(bd, 5);
    case FLOAT_S:    return to_float_s(bd);
    case BOOL_T:     return to_bool_gen(bd, Py_True);
    case BOOL_F:     return to_bool_gen(bd, Py_False);
    case COMPLEX_S:  return to_complex_s(bd);
    case NONE_S:     return to_none_s(bd);
    case ELLIPSIS_S: return to_ellipsis_s(bd);
    case BYTES_E:    return to_bytes_e(bd, 0);
    case BYTES_1:    return to_bytes_gen(bd, 1, 0);
    case BYTES_2:    return to_bytes_gen(bd, 2, 0);
    case BYTES_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the length of the length bytes from the 1st character away from the offset
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_bytes_gen(bd, size_bytes_length, 0);
    }
    case BYTEARR_E: return to_bytes_e(bd, 1);
    case BYTEARR_1: return to_bytes_gen(bd, 1, 1);
    case BYTEARR_2: return to_bytes_gen(bd, 2, 1);
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
    case DECIMAL_1: return to_decimal_gen(bd, 1);
    case DECIMAL_2: return to_decimal_gen(bd, 2);
    case DECIMAL_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        // Get the size of the size bytes
        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_decimal_gen(bd, size_bytes_length);
    }
    case LIST_E: return to_list_e(bd);
    case LIST_1: return to_list_gen(bd, 1);
    case LIST_2: return to_list_gen(bd, 2);
    case LIST_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_list_gen(bd, size_bytes_length);
    }
    case TUPLE_E: return to_tuple_e(bd);
    case TUPLE_1: return to_tuple_gen(bd, 1);
    case TUPLE_2: return to_tuple_gen(bd, 2);
    case TUPLE_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_tuple_gen(bd, size_bytes_length);
    }
    case SET_E: return to_set_frozenset_e(bd, 0);
    case SET_1: return to_set_frozenset_gen(bd, 1, 0);
    case SET_2: return to_set_frozenset_gen(bd, 2, 0);
    case SET_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_set_frozenset_gen(bd, size_bytes_length, 0);
    }
    case FSET_E: return to_set_frozenset_e(bd, 1);
    case FSET_1: return to_set_frozenset_gen(bd, 1, 1);
    case FSET_2: return to_set_frozenset_gen(bd, 2, 1);
    case FSET_D:
    {
        if (ensure_offset(bd, 1) == -1) return NULL;

        size_t size_bytes_length = bytes_to_size_t(&(bd->bytes[++bd->offset]), 1);
        // Use and return the generic method
        return to_set_frozenset_gen(bd, size_bytes_length, 1);
    }
    case DICT_E: return to_dict_e(bd);
    case DICT_1: return to_dict_gen(bd, 1);
    case DICT_2: return to_dict_gen(bd, 2);
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

PyObject *to_value_prot1(PyObject *py_bytes)
{
    // Get the PyBytes object as a C string
    const unsigned char *bytes = (const unsigned char *)PyBytes_AsString(py_bytes);

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

