#define PY_SSIZE_T_CLEAN

#include <Python.h>

/*
  # Datatype characters (datachars)

  Regular (not capitalized*):

  - String:    'j' (Up to 255 bytes)    
  - String:    'k' (Up to 65535 bytes)  
  - String:    'l' (empty string)       
  - String:    's' (dynamic byte size)  
  - Int:       'i' (dynamic byte size)  
  - Int:       'a' (1 byte)             
  - Int:       'd' (2 bytes)            
  - Int:       'g' (3 bytes)            
  - Int:       'h' (4 bytes)            
  - Int:       'm' (5 bytes)            
  - Float:     'f' (dynamic byte size)  
  - Bool:      'x' (True value)         
  - Bool:      'y' (False value)        
  - Complex:   'c' (constant byte size) 
  - None:      'n' *(or 'N', because `type(None)` returns capital N) (0 bytes, static value)
  - Ellipsis:  'e' (0 bytes, static value)

  Iterable/dict (capitalized):

  - List:      'L'
  - Tuple:     'T'
  - Set:       'S'
  - Frozenset: 'F'
  - Dict:      'D'
  - Empty:     'P' (Follows after an empty iterable/dict)
*/

inline PyObject *unsigned_to_bytes(Py_ssize_t value)
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

inline size_t bytes_to_size_t(const unsigned char *bytes, size_t length)
{
    size_t num = 0;

    for (size_t i = 0; i < length; ++i)
    {
        num += ((size_t)bytes[i]) << (i * 8);
    }

    return num;
}

PyObject *specialized_from_value(PyObject *value, char datachar, int add_metadata)
{
    switch (datachar)
    {
    case 's': // String
    {
        if (!PyUnicode_Check(value))
        {
            PyErr_SetString(PyExc_ValueError, "Value of type 'str' expected.");
            return NULL;
        }

        PyObject *pybytes = PyUnicode_AsEncodedString(value, "utf-8", "strict");

        if (add_metadata)
        {
            // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
            Py_ssize_t size = PyBytes_Size(pybytes);
            PyObject *length_bytes = unsigned_to_bytes(size);
            PyObject *metabytes = NULL; // This will hold the metadata for the bytes

            if (size == 0) // Check if it's empty
            {
                Py_DECREF(length_bytes);
                Py_XDECREF(metabytes);
                Py_DECREF(pybytes);
                // Return just the datachar for an empty string
                return PyBytes_FromStringAndSize("l", 1);
            }
            else if (size < 256) // Check whether we can store the size in a single byte
            {
                // Get a PyBytes object of the string 255 datachar
                metabytes = PyBytes_FromStringAndSize("j", 1);
            }
            else if (size < 65536) // Or two bytes (2 ^ 16 - 1 = 65535)
            {
                // Get a PyBytes object of the string 65535 datachar
                metabytes = PyBytes_FromStringAndSize("k", 1);
            }
            else
            {
                // Get a PyBytes object of the string dynamic datachar
                metabytes = PyBytes_FromStringAndSize("s", 1);

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
        else
        {
            return pybytes;
        }
    }
    case 'i': // Integer
    {
        if (!PyLong_Check(value))
        {
            PyErr_SetString(PyExc_ValueError, "Value of type 'int' expected.");
            return NULL;
        }

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

        // Check if we need to add metadata
        if (add_metadata)
        {
            PyObject *metabytes = NULL;

            switch (num_bytes)
            {
            case 1:
            {
                // Make a bytes object from the datachar
                metabytes = PyBytes_FromStringAndSize("a", 1);
                break;
            }
            case 2:
            {
                metabytes = PyBytes_FromStringAndSize("d", 1);
                break;
            }
            case 3:
            {
                metabytes = PyBytes_FromStringAndSize("g", 1);
                break;
            }
            case 4:
            {
                metabytes = PyBytes_FromStringAndSize("h", 1);
                break;
            }
            case 5:
            {
                metabytes = PyBytes_FromStringAndSize("m", 1);
                break;
            }
            default:
            {
                // Get the size of the byte array as a Py_ssize_t and convert the size to bytes
                Py_ssize_t size = PyByteArray_Size(pybytes);
                PyObject *length_bytes = unsigned_to_bytes(size);

                // Get a PyBytes object of the integer dynamic datachar
                metabytes = PyBytes_FromStringAndSize("i", 1);

                // Concat the length bytes on top
                PyBytes_ConcatAndDel(&metabytes, length_bytes);
                break;
            }
            }
            // Add the number bytes on top
            PyBytes_ConcatAndDel(&metabytes, pybytes);

            return metabytes;
        }
        else
        {
            // Return the bytes converted normally
            return pybytes;
        }
    }
    case 'f': // Float
    {
        if (!PyFloat_Check(value))
        {
            PyErr_SetString(PyExc_ValueError, "Value of type 'float' expected.");
            return NULL;
        }

        double c_num = PyFloat_AsDouble(value);
        unsigned char *bytes = (unsigned char *)malloc(sizeof(double));
        memcpy(bytes, &c_num, sizeof(double));

        PyObject *pybytes = PyBytes_FromStringAndSize((const char *)bytes, sizeof(double));

        free(bytes);

        if (add_metadata)
        {
            // Get a PyBytes object of the float datachar
            PyObject *metabytes = PyBytes_FromStringAndSize("f", 1);
            // Concat the length bytes and the float bytes on top
            PyBytes_ConcatAndDel(&metabytes, pybytes);

            return metabytes;
        }
        else
        {
            return pybytes;
        }
    }
    case 'c': // Complex
    {
        if (!PyComplex_Check(value))
        {
            PyErr_SetString(PyExc_ValueError, "Value of type 'complex' expected.");
            return NULL;
        }

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

        if (add_metadata)
        {
            // Only add the complex type datachar, the length is consistently 16
            PyObject *metabytes = PyBytes_FromStringAndSize("c", 1);
            PyBytes_ConcatAndDel(&metabytes, pybytes);

            return metabytes;
        }
        else
        {
            return pybytes;
        }
    }
    case 'b': // Boolean
    {
        if (!PyBool_Check(value))
        {
            PyErr_SetString(PyExc_ValueError, "Value of type 'bool' expected.");
            return NULL;
        }

        if (PyObject_IsTrue(value))
        {
            return PyBytes_FromStringAndSize("x", 1);
        }

        return PyBytes_FromStringAndSize("y", 1);
    }
    case 'n': // NoneType
    case 'N':
    {
        if (add_metadata)
        {
            return PyBytes_FromStringAndSize("n", 1);
        }

        return PyBytes_FromStringAndSize(NULL, 0);
    }
    case 'e': // Ellipsis
    {
        if (add_metadata)
        {
            return PyBytes_FromStringAndSize("e", 1);
        }

        return PyBytes_FromStringAndSize(NULL, 0);
    }
    default:
    {
        // Datatype was not supported
        PyErr_SetString(PyExc_ValueError, "Received an unsupported datatype.");
        return NULL;
    }
    }
}

PyObject *from_single_value(PyObject *self, PyObject *args)
{
    PyObject *value = NULL;
    PyObject *datatype = NULL;

    if (!PyArg_ParseTuple(args, "OO!", &value, &PyUnicode_Type, &datatype))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 'any' and 'str' type.");
        return NULL;
    }

    Py_INCREF(value);
    Py_INCREF(datatype);

    // Get the first character of the datatype received
    const char datachar = PyUnicode_AsUTF8(datatype)[0];

    PyObject *result = specialized_from_value(value, datachar, 0);

    Py_INCREF(result);

    Py_DECREF(value);
    Py_DECREF(datatype);

    return result;
}

PyObject *__to_single_value(PyObject *bytearr, char datachar)
{
    switch (datachar)
    {
    case 's': // String
    {
        PyObject *result =  PyUnicode_FromEncodedObject(bytearr, "utf-8", "strict");

        Py_DECREF(bytearr);

        return result;
    }
    case 'i': // Integer
    {
        // Get the size and data of the bytes object
        Py_ssize_t num_bytes = PyBytes_Size(bytearr);
        unsigned char *bytes = (unsigned char *)PyBytes_AsString(bytearr);

        if (bytes == NULL)
        {
            Py_XDECREF(bytearr);
            return NULL;
        }

        // Create a Python integer from the byte array
        PyObject *result = _PyLong_FromByteArray(bytes, num_bytes, 1, 1);

        Py_DECREF(bytearr);

        return result;
    }
    case 'f': // Float
    {
        const char *bytes_data = PyBytes_AsString(bytearr);
        if (!bytes_data)
        {
            Py_XDECREF(bytearr);
            return NULL;
        }

        double value;
        memcpy(&value, bytes_data, sizeof(double));

        Py_DECREF(bytearr);

        return PyFloat_FromDouble(value);
    }
    case 'c': // Complex
    {
        const char *bytes_data = PyBytes_AsString(bytearr);
        if (!bytes_data)
        {
            Py_XDECREF(bytearr);
            return NULL;
        }

        double real, imag;
        memcpy(&real, bytes_data, sizeof(double));
        memcpy(&imag, bytes_data + sizeof(double), sizeof(double));

        Py_DECREF(bytearr);

        Py_complex c;
        c.real = real;
        c.imag = imag;

        return PyComplex_FromCComplex(c);
    }
    case 'b': // Boolean
    {
        const char *bytes = PyBytes_AsString(bytearr);
        return PyBool_FromLong(bytes[0] == 'x');
    }
    case 'n': // Nonetype
    {
        Py_XDECREF(bytearr);
        return Py_None;
    }
    case 'N': // Nonetype
    {
        Py_XDECREF(bytearr);
        return Py_None;
    }
    case 'e': // Ellipsis
    {
        Py_XDECREF(bytearr);
        return Py_Ellipsis;
    }
    default:
    {
        Py_XDECREF(bytearr);
        PyErr_SetString(PyExc_ValueError, "Unsupported datatype received.");
        return NULL;
    }
    }
}

PyObject *to_single_value(PyObject *self, PyObject *args)
{
    PyObject *bytearr = NULL;
    PyObject *datatype = NULL;

    if (!PyArg_ParseTuple(args, "OO!", &bytearr, &PyUnicode_Type, &datatype))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 'any' and 'str' type.");
        return NULL;
    }

    Py_INCREF(bytearr);
    Py_INCREF(datatype);

    // Get the first character of the datatype
    const char *datachars = PyUnicode_AsUTF8(datatype);
    PyObject *result = __to_single_value(bytearr, datachars[0]);

    Py_DECREF(bytearr);
    Py_DECREF(datatype);

    return result;
}

// Returns NULL if not an iterable, else a PyList of the iterable
inline PyObject *iterable_to_list(PyObject *iterable)
{
    if (PyList_Check(iterable))
    {
        Py_INCREF(iterable); // Increment reference if returning the same list
        return iterable;
    }
    else if (PySet_Check(iterable) || PyFrozenSet_Check(iterable) || PyTuple_Check(iterable))
    {
        return PySequence_List(iterable); // This returns a new reference
    }
    else
    {
        return NULL; // Return NULL to indicate failure
    }
}

typedef struct
{
    PyObject *keys;
    PyObject *values;
} KeyValuePair;

inline KeyValuePair *separate_dict(PyObject *dict)
{
    if (!PyDict_Check(dict))
    {
        PyErr_SetString(PyExc_ValueError, "Provided item must be of type 'dict'.");
        return NULL;
    }

    // Check whether the list is empty and return NULL if so
    if (!PyDict_Size(dict))
    {
        return NULL;
    }

    KeyValuePair *pair = (KeyValuePair *)malloc(sizeof(KeyValuePair));
    if (!pair)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for KeyValuePair.");
        return NULL;
    }

    pair->keys = PyDict_Keys(dict);     // New reference
    pair->values = PyDict_Values(dict); // New reference

    if (!pair->keys || !pair->values)
    {
        Py_XDECREF(pair->keys);
        Py_XDECREF(pair->values);
        free(pair);
        return NULL;
    }

    return pair;
}

// Main serialization function for lists and iterables
PyObject *__from_list(PyObject *value, char datachar, int add_sizedata)
{
    datachar = toupper(datachar);

    Py_ssize_t it_len = PyList_Size(value);

    // Check if it's empty
    if (!it_len)
    {
        // Return a dict datachar with an 'empty' datachar
        return PyBytes_FromStringAndSize("LP", 2);
    }

    PyObject **items = (PyObject **)malloc(it_len * sizeof(PyObject *));

    if (!items)
    {
        free(items);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for items array.");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < it_len; i++)
    {
        PyObject *item = PyList_GET_ITEM(value, i);

        // Store type name and first character for reuse
        PyTypeObject *item_type = Py_TYPE(item);

        const char *item_datatype = item_type->tp_name;
        const char item_datachar = item_datatype[0];

        PyObject *item_list = iterable_to_list(item);

        if (item_list)
        {
            PyObject *item_bytes = __from_list(item_list, item_datachar, 1);

            items[i] = item_bytes;

            Py_DECREF(item_list);
        }
        else if (PyDict_Check(item))
        {
            KeyValuePair *pair = separate_dict(item);

            // If we didn't receive a pair, the dict is empty
            if (!pair)
            {
                // Add a dict datachar with an 'empty' datachar
                items[i] = PyBytes_FromStringAndSize("DP", 2);
                continue;
            }

            PyObject *key_bytes = __from_list(pair->keys, 'L', 1);
            PyObject *value_bytes = __from_list(pair->values, 'L', 1);

            if (!key_bytes || !value_bytes)
            {
                Py_XDECREF(key_bytes);
                Py_XDECREF(value_bytes);

                for (Py_ssize_t j = 0; j < i; j++)
                {
                    Py_XDECREF(items[j]);
                }

                Py_DECREF(pair->keys);
                Py_DECREF(pair->values);

                free(pair);
                free(items);

                return NULL;
            }

            PyObject *item_bytes = PyBytes_FromStringAndSize("D", 1);

            PyBytes_ConcatAndDel(&item_bytes, key_bytes);
            PyBytes_ConcatAndDel(&item_bytes, value_bytes);

            Py_DECREF(pair->keys);
            Py_DECREF(pair->values);

            free(pair);

            items[i] = item_bytes;
        }
        else
        {
            PyObject *item_bytes = specialized_from_value(item, item_datachar, 1);
            if (!item_bytes)
            {
                for (Py_ssize_t j = 0; j < i; j++)
                {
                    Py_DECREF(items[j]);
                }

                free(items);

                return NULL;
            }

            items[i] = item_bytes;
        }
    }

    PyObject *bytes = PyBytes_FromStringAndSize(&datachar, 1);
    if (add_sizedata)
    {
        PyObject *size_bytes = unsigned_to_bytes(it_len);
        Py_ssize_t size_bytes_len = PyBytes_Size(size_bytes);
        PyObject *size_bytes_bytes = unsigned_to_bytes(size_bytes_len);

        PyBytes_ConcatAndDel(&bytes, size_bytes_bytes);
        PyBytes_ConcatAndDel(&bytes, size_bytes);
    }

    for (Py_ssize_t i = 0; i < it_len; i++)
    {
        PyBytes_ConcatAndDel(&bytes, items[i]);
    }

    free(items);

    return bytes;
}

PyObject *from_value(PyObject *self, PyObject *args)
{
    PyObject *value = NULL;

    if (!PyArg_ParseTuple(args, "O", &value))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'any' type.");
        return NULL;
    }

    Py_INCREF(value);

    const char *datatype = Py_TYPE(value)->tp_name;
    char datachar = datatype[0];

    PyObject *list = iterable_to_list(value);
    if (list)
    {
        PyObject *result = __from_list(list, datachar, 1);
        Py_DECREF(list);
        Py_DECREF(value);
        return result;
    }
    else if (PyDict_Check(value))
    {
        KeyValuePair *pair = separate_dict(value);

        // Return empty dict if NULL was returned
        if (!pair)
        {
            return PyBytes_FromStringAndSize("DP", 2);
        }

        PyObject *key_bytes = __from_list(pair->keys, 'L', 1);
        PyObject *value_bytes = __from_list(pair->values, 'L', 1);

        if (!key_bytes || !value_bytes)
        {
            Py_XDECREF(key_bytes);
            Py_XDECREF(value_bytes);
            Py_DECREF(pair->keys);
            Py_DECREF(pair->values);
            Py_DECREF(value);
            free(pair);
            return NULL;
        }

        PyObject *bytes = PyBytes_FromStringAndSize("D", 1);
        PyBytes_ConcatAndDel(&bytes, key_bytes);
        PyBytes_ConcatAndDel(&bytes, value_bytes);

        Py_DECREF(pair->keys);
        Py_DECREF(pair->values);
        Py_DECREF(value);
        free(pair);
        return bytes;
    }
    else
    {
        PyObject *value_bytes = specialized_from_value(value, datachar, 1);

        Py_DECREF(value);
        if (!value_bytes)
        {
            return NULL;
        }
        return value_bytes;
    }
}

inline int is_iterable(const char datachar)
{
    // Check whether the received datachar is a datachar of an iterable
    return (datachar == 'L' || datachar == 'T' || datachar == 'S' || datachar == 'F');
}

inline int requires_bytes(const char datachar)
{
    // Check whether the datachar is that of a static value, thus not requiring bytes
    return (datachar != 'n' && datachar != 'N' && datachar != 'e' && datachar != 'b');
}

inline PyObject *to_actual_iterable(PyObject *list, size_t it_len, const char datachar)
{
    // First check if it's just a list
    if (datachar == 'L')
    {
        return list;
    }

    // Convert the iterable size to a Py_ssize_t
    Py_ssize_t size = PyLong_AsSsize_t(PyLong_FromSize_t(it_len));

    // This will hold the new iterable
    PyObject *it = NULL;

    switch (datachar)
    {
        case 'T':
        {
            // Create a new object of the actual iterable type
            it = PyTuple_New(it_len);

            // Go over all items from the list and add them to the tuple
            for (Py_ssize_t i = 0; i < size; i++)
            {
                PyObject *item = PyList_GetItem(list, i);
                Py_INCREF(item);
                PyTuple_SetItem(it, i, item);
            }
            break;
        }
        case 'S':
        {
            // Create a set out of the list and return it
            it = PySet_New(list);
            break;
        }
        case 'F':
        {
            // Create a frozenset out of the list and return it
            it = PyFrozenSet_New(list);
            break;
        }
    }

    Py_DECREF(list);

    return it;
}

inline PyObject *specialized_to_integer(char *bytes, size_t *offset, size_t byte_length)
{
    // Create a Python integer from the bytes object
    PyObject *value = _PyLong_FromByteArray((unsigned char *)bytes + 1 + *offset, byte_length, 1, 1);

    // Update the offset
    *offset += 1 + byte_length;

    return value;
}

PyObject *specialized_to_value(char *bytes, const char datachar, size_t *offset)
{
    switch (datachar)
    {
    case 'l': // Empty string
    {
        // Add 1 to offset for the datachar
        (*offset)++;
        // Return an empty string
        return PyUnicode_FromStringAndSize(NULL, 0);
    }
    case 'j': // String (up to 255 bytes)
    {
        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&bytes[*offset + 1], 1);

        // Update the offset to the start of the item bytes
        *offset += 2;

        // Copy the value bytes into a PyBytes object
        PyObject *item_bytes = PyBytes_FromStringAndSize(&bytes[*offset], length);

        // Update the offset to start at the next item
        *offset += length;

        // Get the value of the bytes
        PyObject *value = PyUnicode_FromEncodedObject(item_bytes, "utf-8", "strict");

        Py_DECREF(item_bytes);

        return value;
    }
    case 'k': // String (up to 65535 bytes)
    {
        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&bytes[*offset + 1], 2);

        // Update the offset to the start of the item bytes
        *offset += 3;

        // Copy the value bytes into a PyBytes object
        PyObject *item_bytes = PyBytes_FromStringAndSize(&bytes[*offset], length);

        // Update the offset to start at the next item
        *offset += length;

        // Get the value of the bytes
        PyObject *value = PyUnicode_FromEncodedObject(item_bytes, "utf-8", "strict");

        Py_DECREF(item_bytes);

        return value;
    }
    case 's': // String (dynamic size)
    {
        // Get the length of the length bytes from the 1st character away from the offset
        size_t length_bytes_len = bytes_to_size_t(&bytes[*offset + 1], 1);

        // Get the length of the actual value
        size_t length = bytes_to_size_t(&bytes[*offset + 2], length_bytes_len);

        // Update the offset to the start of the value bytes
        *offset += 2 + length_bytes_len;

        // Get the actual value bytes using the length
        PyObject *item_bytes = PyBytes_FromStringAndSize(&bytes[*offset], length);

        // Convert the value
        PyObject *value = PyUnicode_FromEncodedObject(item_bytes, "utf-8", "strict");

        Py_DECREF(item_bytes);

        // Update the offset to the end of the value bytes
        *offset += length;

        return value;
    }
    case 'i': // Integer (dynamic size)
    {
        // Get the length of the item bytes
        size_t length = bytes_to_size_t(&bytes[*offset + 1], 2);

        // Create a Python integer from the bytes object
        PyObject *value = _PyLong_FromByteArray((const unsigned char *)&bytes[*offset + 2], length, 1, 1);

        // Update the offset to start at the next item
        *offset += 2 + length;

        return value;
    }
    case 'a': // Integer (1 byte)
    {
        return specialized_to_integer(bytes, offset, 1);
    }
    case 'd': // Integer (2 bytes)
    {
        return specialized_to_integer(bytes, offset, 2);
    }
    case 'g': // Integer (3 bytes)
    {
        return specialized_to_integer(bytes, offset, 3);
    }
    case 'h': // Integer (4 bytes)
    {
        return specialized_to_integer(bytes, offset, 4);
    }
    case 'm': // Integer (5 bytes)
    {
        return specialized_to_integer(bytes, offset, 5);
    }
    case 'f': // Float
    {
        // Set up a double and copy the bytes to it
        double value;
        memcpy(&value, &bytes[*offset + 1], sizeof(double));

        // Update the offset to start at the next item
        *offset += sizeof(double) + 1;

        return PyFloat_FromDouble(value);
    }
    case 'x': // Boolean (True)
    {
        // Update the offset to skip over the datachar byte
        (*offset)++;
        return Py_True;
    }
    case 'y': // Boolean (False)
    {
        (*offset)++;
        return Py_False;
    }
    case 'c': // Complex
    {
        // Update the offset to start at the item bytes
        (*offset)++;

        double real, imag;
        memcpy(&real, &bytes[*offset], sizeof(double));
        memcpy(&imag, &bytes[*offset + sizeof(double)], sizeof(double));

        // Update the offset to start at the next items bytes
        *offset += 2 * sizeof(double);

        // Create a complex object and set the real and imaginary values
        Py_complex c;
        c.real = real;
        c.imag = imag;

        // Return the complex object as a PyComplex
        return PyComplex_FromCComplex(c);
    }
    case 'n': // NoneType
    {
        (*offset)++;
        return Py_None;
    }
    case 'e': // Ellipsis
    {
        (*offset)++;
        return Py_Ellipsis;
    }
    default:
    {
        // Invalid datachar received
        PyErr_SetString(PyExc_ValueError, "Received an invalid byte representative.");
        return NULL;
    }
    }
}

PyObject *__to_list(char *bytes, const char datachar, size_t *offset)
{
    // Check whether the list is empty
    if (bytes[*offset + 1] == 'P')
    {
        *offset += 2;
        return to_actual_iterable(PyList_New(0), 0, datachar);
    }

    // Get the length of the amount bytes from the 1st character away from the offset
    size_t amt_bytes_len = bytes_to_size_t(&bytes[*offset + 1], 1);

    // Get the amount of items to look for
    size_t item_amt = bytes_to_size_t(&bytes[*offset + 2], amt_bytes_len);

    // Update the offset to the start of the first item
    *offset += 2 + amt_bytes_len;

    // Get a new Python list to add the converted items to
    PyObject *list = PyList_New(item_amt);

    for (size_t i = 0; i < item_amt; i++)
    {
        // Get the datachar of the value, located directly after the length bytes
        const char item_datachar = bytes[*offset];
        PyObject *item = NULL;

        if (is_iterable(item_datachar))
        {
            item = __to_list(bytes, item_datachar, offset);
        }
        else if (item_datachar == 'D')
        {
            // Check if it's an empty dict
            if (bytes[*offset + 1] == 'P')
            {
                *offset += 2;
                // Add an empty dict and continue
                PyList_SetItem(list, i, PyDict_New());
                continue;
            }

            // Increment offset by 1 to skip over the dict databyte
            *offset += 1;

            // Get the keys and the values, stored as a stacked list
            PyObject *keys = __to_list(bytes, 'L', offset);
            PyObject *values = __to_list(bytes, 'L', offset);

            // Create a new dict object
            item = PyDict_New();

            // Go over all items and add them to the dict
            for (Py_ssize_t i = 0; i < PyList_Size(keys); i++)
            {
                // Grab the key and the value
                PyObject *key = PyList_GetItem(keys, i);
                PyObject *value = PyList_GetItem(values, i);
                // And add them to the dict
                PyDict_SetItem(item, key, value);
            }

            Py_DECREF(keys);
            Py_DECREF(values);
        }
        else
        {
            item = specialized_to_value(bytes, item_datachar, offset);
        }
        
        if (item == NULL)
        {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SetItem(list, i, item);
    }

    return to_actual_iterable(list, item_amt, datachar);
}

PyObject *__to_dict(char *bytes, size_t *offset)
{
    if (bytes[*offset + 1] == 'P')
    {
        *offset += 2;
        return PyDict_New();
    }
    // Increment offset by 1 to skip over the dict databyte
    *offset += 1;

    // Get the keys and the values, stored as a stacked list
    PyObject *keys = __to_list(bytes, 'L', offset);
    PyObject *values = __to_list(bytes, 'L', offset);

    // Create a new dict object
    PyObject *dict = PyDict_New();

    // Go over all items and add them to the dict
    for (Py_ssize_t i = 0; i < PyList_Size(keys); i++)
    {
        // Grab the key and the value
        PyObject *key = PyList_GetItem(keys, i);
        PyObject *value = PyList_GetItem(values, i);
        // And add them to the dict
        PyDict_SetItem(dict, key, value);
    }

    Py_DECREF(keys);
    Py_DECREF(values);

    return dict;
}

PyObject *to_list(PyObject *py_bytes)
{
    char *bytes = PyBytes_AsString(py_bytes);

    size_t offset = 0;

    if (is_iterable(bytes[0]))
    {
        return __to_list(bytes, bytes[0], &offset);
    }

    return __to_dict(bytes, &offset);
}

PyObject *to_value(PyObject *self, PyObject *args)
{
    PyObject *py_bytes = NULL;

    if (!PyArg_ParseTuple(args, "O", &py_bytes))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'bytes' type.");
        return NULL;
    }

    Py_INCREF(py_bytes);

    char *bytes = PyBytes_AsString(py_bytes);

    // Check if the bytes object is a single value
    if (!is_iterable(bytes[0]) && bytes[0] != 'D')
    {
        // Get the datachar of the value
        const char datachar = bytes[0];

        // Create a temporary offset to pass along
        size_t offset = 0;

        // Return the return value of the converter
        PyObject *value = specialized_to_value(bytes, datachar, &offset);

        Py_DECREF(py_bytes);

        return value;
    }

    PyObject *result = to_list(py_bytes);

    return result;
}

static PyMethodDef methods[] = {
    {"from_single_value", from_single_value, METH_VARARGS, "Convert a non-list value to bytes without the overhead of the regular from_value, if you know the datatype."},
    {"to_single_value", to_single_value, METH_VARARGS, "Convert a non-list bytes object to its value without the overhead of the regular to_value, if you know the datatype."},

    {"from_value", from_value, METH_VARARGS, "Convert a value to a bytes object."},
    {"to_value", to_value, METH_VARARGS, "Convert a bytes object to a value."},

    {NULL, NULL, 0, NULL}
};

// Finalize the Python interpreter on exit
void pybytes_module_cleanup(void *module)
{
    Py_Finalize();
}

static struct PyModuleDef pybytes = {
    PyModuleDef_HEAD_INIT,
    "pybytes",
    "A module for converting values to bytes and vice versa.",
    -1,
    methods,
    NULL, NULL, NULL,
    pybytes_module_cleanup
};

PyMODINIT_FUNC PyInit_pybytes(void)
{
    Py_Initialize();
    return PyModule_Create(&pybytes);
}

