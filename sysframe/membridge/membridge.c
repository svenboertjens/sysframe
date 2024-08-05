#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <pthread.h>

// Include the serialization function from pybytes (from_value, to_value)
#include "../pybytes/sbs_main/sbs_2.h"

// Struct for basic shared memory
typedef struct {
    size_t max_size;
    pthread_mutex_t mutex;
} BasicShm;

// The default size for basic shared memory
#define BASIC_SIZE sizeof(BasicShm)

// The headroom size for not too frequent reallocs
#define HEAD_SIZE 32

// # Shared memory creation & setup

static inline int create_shared_memory(const char *name, size_t pre_size, PyObject *error_if_exists)
{
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1)
    {
        if (errno == EEXIST && error_if_exists && Py_IsTrue(error_if_exists))
        {
            PyErr_Format(PyExc_MemoryError, "The memory address '%s' already exists.", name);
            return -1; // Return -1 to indicate failure with error
        }
        // Return 1 to indicate failure without error
        return 1;
    }

    if (ftruncate(fd, BASIC_SIZE + pre_size) == -1)
    {
        close(fd);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to allocate for shared memory address '%s'.", name);
        return -1;
    }

    BasicShm *shm = mmap(NULL, BASIC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED)
    {
        close(fd);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to map shared memory address '%s'.", name);
        return -1;
    }

    // Initiate the mutex
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0 ||
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_mutex_init(&(shm->mutex), &attr) != 0)
    {
        munmap(shm, BASIC_SIZE);
        close(fd);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to initialize mutex for shared memory address '%s'.", name);
        return -1;
    }

    shm->max_size = pre_size;
    pthread_mutexattr_destroy(&attr);
    munmap(shm, BASIC_SIZE);
    close(fd);

    // Return 0 to indicate success
    return 0;
}

PyObject *create_memory(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *name;
    PyObject *prealloc_size = NULL;
    PyObject *error_if_exists = NULL;

    static char* kwlist[] = {"name", "prealloc_size", "error_if_exists", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O!O!", kwlist, &name, &PyLong_Type, &prealloc_size, &PyBool_Type, &error_if_exists))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the name (str) argument.");
        return NULL;
    }

    size_t pre_size = 0;
    if (prealloc_size != NULL)
    {
        pre_size = PyLong_AsSize_t(prealloc_size);
        if (pre_size == (size_t)-1 && PyErr_Occurred())
        {
            PyErr_SetString(PyExc_ValueError, "The given pre-allocate size is too large.");
            return NULL;
        }
    }

    int result = create_shared_memory(name, pre_size, error_if_exists);
    switch(result)
    {
    case -1: return NULL; // Error already set
    case 0:  return Py_True;
    case 1:  return Py_False;
    }
}

// Helper function to get a shared memory pointer
static inline BasicShm *get_shm(const char *name, size_t new_size, PyObject *create)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        if (errno == ENOENT && (create == NULL || (create && Py_IsTrue(create))))
        {
            if (create_shared_memory(name, 0, NULL) == -1)
                return NULL;
            fd = shm_open(name, O_RDWR, 0666);
            if (fd == -1)
            {
                PyErr_Format(PyExc_MemoryError, "Failed to open shared memory address '%s' after creation.", name);
                return NULL;
            }
        }
        else
        {
            PyErr_Format(PyExc_MemoryError, "Failed to open shared memory address '%s'.", name);
            return NULL;
        }
    }

    // Map the basic structure first to access max_size
    BasicShm *shm = mmap(NULL, BASIC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED)
    {
        close(fd);
        PyErr_Format(PyExc_MemoryError, "Failed to map shared memory metadata address '%s'.", name);
        return NULL;
    }

    // Get the max size from the basic structure
    size_t max_size = shm->max_size;
    size_t total_size = BASIC_SIZE + max_size;
    
    // Unmap the initial mapping to remap the full size
    munmap(shm, BASIC_SIZE);

    // Check whether we got a new size we might need to update
    if (new_size > max_size)
    {
        // Update the new total size
        total_size = BASIC_SIZE + new_size + HEAD_SIZE;

        if (ftruncate(fd, total_size) == -1)
        {
            close(fd);
            PyErr_Format(PyExc_MemoryError, "Failed to resize shared memory.");
            return NULL;
        }
    }

    // Remap with the correct size
    shm = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED)
    {
        close(fd);
        PyErr_Format(PyExc_MemoryError, "Failed to map shared memory address '%s'.", name);
        return NULL;
    }

    // Update the max size if it was changed
    if (shm->max_size < new_size)
        shm->max_size = new_size + HEAD_SIZE;

    close(fd);
    return shm;
}

// Function to close a shared memory pointer
static inline void close_shm(BasicShm *shm)
{
    size_t total_size = BASIC_SIZE + shm->max_size;
    pthread_mutex_unlock(&(shm->mutex));
    munmap(shm, total_size);
}

PyObject *remove_memory(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *name;
    PyObject *throw_error = NULL;

    static char* kwlist[] = {"name", "throw_error", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O!", kwlist, &name, &PyBool_Type, &throw_error))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'name' (str) argument.");
        return NULL;
    }

    if (shm_unlink(name) == -1)
    {
        if (throw_error && Py_IsTrue(throw_error))
        {
            PyErr_SetString(PyExc_MemoryError, "Failed to unlink the shared memory.");
            return NULL;
        }
        Py_RETURN_FALSE;
    }

    Py_RETURN_TRUE;
}

PyObject *read_memory(PyObject *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    BasicShm *shm = get_shm(name, 0, Py_None);
    if (shm == NULL) return NULL;  // Error already set

    if (shm->max_size == 0)
    {
        close_shm(shm);
        Py_RETURN_NONE;
    }

    PyObject *py_bytes = PyBytes_FromStringAndSize((char *)shm + BASIC_SIZE, shm->max_size);
    PyObject *value = to_value(py_bytes);

    Py_DECREF(py_bytes);
    close_shm(shm);

    return value;
}

PyObject *write_memory(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *name;
    PyObject *value;
    PyObject *create = NULL;

    static char* kwlist[] = {"name", "value", "create", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|O!", kwlist, &name, &value, &PyBool_Type, &create))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'name' (str) and 'value' (any) arguments.");
        return NULL;
    }

    // Convert the value to a Python bytes object
    PyObject *py_bytes = from_value(value);
    if (py_bytes == NULL) return NULL; // Error already set

    // Convert the Python bytes object to C bytes
    size_t size;
    char *bytes;
    if (PyBytes_AsStringAndSize(py_bytes, &bytes, &size) == -1)
    {
        Py_DECREF(py_bytes);
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert a Python bytes object to a C string.");
        return NULL;
    }
    Py_DECREF(py_bytes);

    BasicShm *shm = get_shm(name, size, create);
    if (shm == NULL) return NULL;

    memcpy((char *)shm + BASIC_SIZE, bytes, size);
    close_shm(shm);
    Py_RETURN_TRUE;
}

// # Shared functions

typedef struct {
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t func_cond;
    pthread_cond_t call_cond;
    unsigned char activity;
    unsigned char *args;
} shared_function;

// Initiate a shared memory for a shared function
PyObject *create_shared_function(const char *name, PyObject *func)
{
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1)
    {
        // Check whether it already exists or if it's a different error
        if (errno == EEXIST)
        {
            PyErr_SetString(PyExc_MemoryError, "The shared memory address already exists.");
            return NULL;
        }
        else
        {
            PyErr_SetString(PyExc_MemoryError, "Failed to create the shared memory.");
            return NULL;
        }
    }

    if (ftruncate(fd, sizeof(shared_function)) == -1)
    {
        close(fd);
        shm_unlink(name);
        PyErr_SetString(PyExc_MemoryError, "Failed to set up the shared memory.");
        return NULL;
    }

    void *shm_ptr = mmap(NULL, sizeof(shared_function), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        close(fd);
        shm_unlink(name);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Create the shared function struct
    shared_function *data = shm_ptr;

    // Initialize mutex and cond variables
    pthread_mutex_t *mutex = &(data->mutex);
    pthread_mutexattr_t mutex_attr;

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(mutex, &mutex_attr);

    pthread_cond_t *func_cond = &(data->func_cond);
    pthread_condattr_t func_cond_attr;

    pthread_condattr_init(&func_cond_attr);
    pthread_condattr_setpshared(&func_cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(func_cond, &func_cond_attr);

    pthread_cond_t *call_cond = &(data->call_cond);
    pthread_condattr_t call_cond_attr;

    pthread_condattr_init(&call_cond_attr);
    pthread_condattr_setpshared(&call_cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(call_cond, &call_cond_attr);

    // Set the activity byte to 0 to indicate it's active
    data->activity = 0;

    // Set a while loop to repeatedly catch all signals
    while (1) {
        // Begin waiting on the cond to be signaled
        pthread_mutex_lock(mutex);
        pthread_cond_wait(func_cond, mutex);

        // Check if the activity byte is set to inactive
        if (data->activity == 1) {
            // Break the loop to do cleanup
            break;
        }

        char *args_bytes = data->args;

        // Convert it to a Python object
        PyObject *py_args = to_value(PyBytes_FromString(args_bytes));

        // Call the function and parse the args to it
        PyObject *returned_args = PyObject_CallObject(func, py_args);
        char *returned_bytes = PyBytes_AsString(from_value(py_args));

        // Copy the returned args to the buffer so that the caller can fetch them
        strncpy(data->args, returned_bytes, 1023);
        data->args[1023] = '\0'; // Ensure null-termination

        // Signal the caller that the returned args are set
        pthread_cond_signal(call_cond);

        // Unlock the mutex after performing the function call to prevent signals from missing this loop
        pthread_mutex_unlock(mutex);
    }

    // Cleanup for when the loop breaks
    munmap(shm_ptr, sizeof(shared_function));
    close(fd);

    // Return True to avoid getting a null-exception message
    return Py_True;
}

PyObject *create_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *func;

    if (!PyArg_ParseTuple(args, "O!O", &PyUnicode_Type, &name, &func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'callable' type.");
        return NULL;
    }

    if (!PyCallable_Check(func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'callable' type.");
        return NULL;
    }

    Py_INCREF(name);
    Py_INCREF(func);

    // Call the function to create the shared function
    PyObject *return_value = create_shared_function(PyUnicode_AsUTF8(name), func);

    Py_DECREF(name);
    Py_DECREF(func);

    return return_value;
}

// Call a function linked to a shared memory conditional
PyObject *call_shared_function(const char *name, PyObject *args)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to open the shared memory.");
        return NULL;
    }

    // Get the full shared memory now that we have the full size
    void *shm_ptr = mmap(NULL, sizeof(shared_function), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        close(fd);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Get the data itself from the shm of the data
    shared_function *data = shm_ptr;

    pthread_mutex_lock(&(data->mutex));

    const char *args_bytes = PyBytes_AsString(from_value(args));

    // Assign the args to the shared memory buffer
    strncpy(data->args, args_bytes, 1023);
    data->args[1023] = '\0'; // Ensure null-termination

    // Signal the halted thread
    pthread_cond_signal(&(data->func_cond));
    // Wait on the caller signal to fetch the returned args
    pthread_cond_wait(&(data->call_cond), &(data->mutex));

    // Get the returned args as a Python object
    char *returned_bytes = data->args;

    // Convert it to a Python object
    PyObject *returned_value = to_value(PyBytes_FromString(returned_bytes));

    pthread_mutex_unlock(&(data->mutex));
    close(fd);

    // Return the returned args from the called function
    return returned_value;
}

PyObject *call_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *py_args;

    if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &name, &PyTuple_Type, &py_args))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'tuple' type.");
        return NULL;
    }

    Py_INCREF(name);
    Py_INCREF(py_args);

    // Call the shared function and get the returned value
    PyObject *return_value = call_shared_function(PyUnicode_AsUTF8(name), py_args);

    Py_DECREF(name);
    Py_DECREF(py_args);

    // Return the returned value to the user
    return return_value;
}

PyObject *remove_function(PyObject *self, PyObject *args)
{
    PyObject *py_name;

    if (!PyArg_ParseTuple(args, "O!", &PyUnicode_Type, &py_name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    // Convert the name to a C string
    char *name = PyUnicode_AsUTF8(py_name);

    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        // Return 2 because it can't be opened. Throwing an error isn't necessary because we want it to be gone anyway
        return PyLong_FromLong(2);
    }

    void *shm_ptr = mmap(NULL, sizeof(shared_function), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        close(fd);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Get the struct stored in the shared memory
    shared_function *data = shm_ptr;

    // Lock the mutex
    pthread_mutex_lock(&(data->mutex));

    // Set the activity byte to 1 to indicate inactive
    data->activity = 1;

    // Signal the function thread to make it check the activity and exit
    pthread_cond_signal(&(data->func_cond));
    pthread_mutex_unlock(&(data->mutex));

    // Close the shared memory address itself as well
    if (!shm_unlink(name))
    {
        // Return 3 to indicate the function is unlinked but the shared memory isn't
        return PyLong_FromLong(3);
    }

    // Return 1 to indicate success
    return PyLong_FromLong(1);
}

static PyMethodDef methods[] = {
    {"create_memory", (PyCFunction)create_memory, METH_VARARGS | METH_KEYWORDS, "Create a shared memory address."},
    {"remove_memory", (PyCFunction)remove_memory, METH_VARARGS | METH_KEYWORDS, "Remove a shared memory address."},
    {"read_memory", read_memory, METH_VARARGS, "Get the value stored in a shared memory address."},
    {"write_memory", (PyCFunction)write_memory, METH_VARARGS | METH_KEYWORDS, "Write a value to a shared memory address."},

    // Still working on these
    //{"create_function", create_function, METH_VARARGS, "Create and link a shared function handle."},
    //{"remove_function", remove_function, METH_VARARGS, "Unlink a shared function and its memory address."},
    //{"call_function", call_function, METH_VARARGS, "Call a function linked to a shared function handle."},

    {NULL, NULL, 0, NULL}
};

void membridge_module_cleanup(void *module)
{
    sbs2_cleanup();
    Py_Finalize();
}

static struct PyModuleDef membridge = {
    PyModuleDef_HEAD_INIT,
    "membridge",
    "A module for using shared memory tools in Python.",
    -1,
    methods,
    NULL, NULL, NULL,
    membridge_module_cleanup
};

PyMODINIT_FUNC PyInit_membridge(void)
{
    sbs2_init();
    Py_Initialize();
    return PyModule_Create(&membridge);
}

