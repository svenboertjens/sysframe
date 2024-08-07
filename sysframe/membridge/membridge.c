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
#include "sbs_main/sbs_2.h"

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
    default:
    {
        PyErr_SetString(PyExc_RuntimeError, "Something went wrong, but we couldn't quite catch what it was.");
        return NULL;
    }
    }
}

// Helper function to get the basic shared memory pointer
static inline BasicShm *get_basic_shm(const char *name, size_t new_size, PyObject *create)
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
    pthread_mutex_unlock(&(shm->mutex));
    munmap(shm, BASIC_SIZE + shm->max_size);
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

    BasicShm *shm = get_basic_shm(name, 0, Py_None);
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
    if (PyBytes_AsStringAndSize(py_bytes, &bytes, (Py_ssize_t *)&size) == -1)
    {
        Py_DECREF(py_bytes);
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert a Python bytes object to a C string.");
        return NULL;
    }
    Py_DECREF(py_bytes);

    BasicShm *shm = get_basic_shm(name, size, create);
    if (shm == NULL) return NULL;

    memcpy((char *)shm + BASIC_SIZE, bytes, size);
    close_shm(shm);
    Py_RETURN_TRUE;
}

// # Shared functions

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t fcond; // Function cond
    pthread_cond_t ccond; // Caller cond
    unsigned char activity;
} FunctionShm;

#define FUNCTION_ARGS 1024 // The static size that will hold the args
#define FUNCTION_SIZE sizeof(FunctionShm)

// Helper function to write a NULL message to function shm
static inline void null_function(FunctionShm *shm)
{
    // Simply set the first arg byte to NULL
    ((char *)shm)[FUNCTION_SIZE] = 0;
}

// Initiate a shared memory for a shared function
static inline PyObject *create_shared_function(const char *name, PyObject *func)
{
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1)
    {
        if (errno == EEXIST)
            PyErr_Format(PyExc_MemoryError, "The memory address '%s' already exists.", name);
        else
            PyErr_Format(PyExc_MemoryError, "Failed to create memory address '%s'.", name);
        
        return NULL;
    }

    if (ftruncate(fd, FUNCTION_SIZE + FUNCTION_ARGS) == -1)
    {
        close(fd);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to allocate for shared memory address '%s'.", name);
        return NULL;
    }

    FunctionShm *shm = mmap(NULL, FUNCTION_SIZE + FUNCTION_ARGS, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED)
    {
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to map shared memory address '%s'.", name);
        return NULL;
    }

    // Initiate the mutex
    pthread_mutexattr_t mutex_attr;
    if (pthread_mutexattr_init(&mutex_attr) != 0 ||
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_mutex_init(&(shm->mutex), &mutex_attr) != 0)
    {
        munmap(shm, BASIC_SIZE);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to initialize mutex for shared memory address '%s'.", name);
        return NULL;
    }
    pthread_mutexattr_destroy(&mutex_attr);

    // Initiate the function cond
    pthread_condattr_t func_attr;
    if (pthread_condattr_init(&func_attr) != 0 ||
        pthread_condattr_setpshared(&func_attr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_cond_init(&(shm->fcond), &func_attr) != 0)
    {
        munmap(shm, BASIC_SIZE);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to initialize signal cond for shared memory address '%s'.", name);
        return NULL;
    }
    pthread_condattr_destroy(&func_attr);

    // Initiate the caller cond
    pthread_condattr_t call_attr;
    if (pthread_condattr_init(&call_attr) != 0 ||
        pthread_condattr_setpshared(&call_attr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_cond_init(&(shm->ccond), &call_attr) != 0)
    {
        munmap(shm, BASIC_SIZE);
        shm_unlink(name);
        PyErr_Format(PyExc_MemoryError, "Failed to initialize signal cond for shared memory address '%s'.", name);
        return NULL;
    }
    pthread_condattr_destroy(&call_attr);

    // Set the activity byte to 1 to indicate activity
    shm->activity = 1;

    // This will hold the exit status, set to 0 on clean exit
    unsigned char exit_status = 1;

    // Set a while loop to repeatedly catch all signals
    while (1) {
        // Begin waiting on the cond to be signaled
        pthread_mutex_lock(&(shm->mutex));
        pthread_cond_wait(&(shm->fcond), &(shm->mutex));

        // Check if the activity byte is set to inactive (0)
        if (shm->activity == 0) {
            printf("read\n");
            // Set the exit status to 1 because we didn't exit with an error
            exit_status = 0;
            // Break the loop to cleanup
            break;
        }

        // Check whether the first byte is NULL, meaning we got a NULL message
        if (*((char *)shm + FUNCTION_SIZE) == 0)
        {
            PyErr_SetString(PyExc_RuntimeError, "Received a NULL message from the caller. This is likely because the caller sent arguments of too large size.");
            break;
        }

        // Convert the args to a Python value
        PyObject *py_args = to_value(PyBytes_FromStringAndSize((char *)shm + FUNCTION_SIZE, FUNCTION_ARGS));

        // This will hold the args to be returned
        PyObject *returned_args = NULL; // NULL by default to return errors on non-success scenarios

        // Check if the args is a tuple
        if (PyTuple_Check(py_args))
            // Set the return args to the functions we receive from the function to call
            if ((returned_args = PyObject_CallObject(func, py_args)) == NULL) return NULL;
        Py_DECREF(py_args);

        // Convert the return args to a char array to store them
        PyObject *py_return = from_value(returned_args);
        Py_XDECREF(returned_args);
        if (py_return == NULL) break; // Error already set

        size_t returned_size;
        char *returned_bytes;
        if (PyBytes_AsStringAndSize(py_return, &returned_bytes, (Py_ssize_t *)&returned_size) == -1)
        {
            Py_DECREF(py_return);
            PyErr_SetString(PyExc_RuntimeError, "Failed to convert a Python bytes object to C bytes.");
            break;
        }
        Py_DECREF(py_return);

        // Check whether the size doesn't exceed the limit
        if (returned_size > FUNCTION_ARGS) break;

        // Set the returned bytes
        memcpy((char *)shm + FUNCTION_SIZE, returned_bytes, returned_size);

        // Signal the caller that the returned args are set
        pthread_cond_signal(&(shm->ccond));
        pthread_mutex_unlock(&(shm->mutex));
    }

    // Check whether we have to clean up some stuff due to an exception that occurred
    if (exit_status == 1)
    {
        printf("exit 1 found\n");
        null_function(shm);
        // Signal the caller and unlock to prevent deadlocks
        pthread_cond_signal(&(shm->ccond));
        pthread_mutex_unlock(&(shm->mutex));
    }
    printf("unlinking\n");

    // Unmap and unlink the shared memory
    munmap(shm, FUNCTION_SIZE + FUNCTION_ARGS);
    shm_unlink(name);
    printf("done\n");

    // Return None on success, NULL on error to throw it
    return exit_status == 1 ? NULL : Py_None;
}

PyObject *create_function(PyObject *self, PyObject *args)
{
    const char *name;
    PyObject *func;

    if (!PyArg_ParseTuple(args, "sO", &name, &func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'callable' type.");
        return NULL;
    }

    if (!PyCallable_Check(func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'callable' type.");
        return NULL;
    }

    // Call the function to create and link the shared function
    Py_INCREF(func);
    PyObject *return_value = create_shared_function(name, func);
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

    // Get the shared memory
    FunctionShm *shm = mmap(NULL, FUNCTION_SIZE + FUNCTION_ARGS, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Lock the mutex
    pthread_mutex_lock(&(shm->mutex));

    // Convert the args to a Python bytes object
    PyObject *py_args = from_value(args);
    if (args == NULL)
    {
        pthread_mutex_unlock(&(shm->mutex));
        munmap(shm, FUNCTION_SIZE + FUNCTION_ARGS);
        return NULL; // Error already set
    }

    // Convert the args to a char array
    size_t arg_size;
    char *arg_bytes;
    if (PyBytes_AsStringAndSize(py_args, &arg_bytes, (Py_ssize_t *)&arg_size) == -1)
    {
        Py_DECREF(py_args);
        pthread_mutex_unlock(&(shm->mutex));
        munmap(shm, FUNCTION_SIZE + FUNCTION_ARGS);
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert the args to C bytes.");
        return NULL;
    }
    Py_DECREF(py_args);

    // Check whether the size doesn't exceed the limit
    if (arg_size > FUNCTION_ARGS)
    {
        pthread_mutex_unlock(&(shm->mutex));
        munmap(shm, FUNCTION_SIZE + FUNCTION_ARGS);
        PyErr_SetString(PyExc_ValueError, "The received args exceed the maximum accepted arg size of 1024 bytes.");
        return NULL;
    }

    // Copy the bytes to the shared memory
    memcpy((char *)shm + FUNCTION_SIZE, arg_bytes, arg_size);

    // Signal the function
    pthread_cond_signal(&(shm->fcond));
    // Wait for the return args
    pthread_cond_wait(&(shm->ccond), &(shm->mutex));

    // Check whether we didn't receive a NULL message
    if (*((char *)shm + FUNCTION_SIZE) == 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Received a NULL message from the function. This is likely because the function returned arguments of too large size.");
        return NULL;
    }

    // Convert the C args to Python bytes
    PyObject *py_return_args = PyBytes_FromStringAndSize((char *)shm + FUNCTION_SIZE, FUNCTION_ARGS);

    // Close the mutex and unmap the shared memory as we no longer need it
    pthread_mutex_unlock(&(shm->mutex));
    munmap(shm, FUNCTION_SIZE + FUNCTION_ARGS);

    if (py_return_args == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert C bytes to a Python bytes object.");
        return NULL;
    }

    // Convert the Python bytes to the Python value we should return, and return it
    PyObject *returned_value = to_value(py_return_args);
    return returned_value;
}

PyObject *call_function(PyObject *self, PyObject *args)
{
    const char *name;
    PyObject *py_args;

    if (!PyArg_ParseTuple(args, "sO!", &name, &PyTuple_Type, &py_args))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'tuple' type.");
        return NULL;
    }

    // Call the shared function and get the returned value
    Py_INCREF(py_args);
    PyObject *return_value = call_shared_function(name, py_args);
    Py_DECREF(py_args);

    // Return the returned value to the user
    return return_value;
}

PyObject *remove_function(PyObject *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s|O!", &name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) return Py_False; // Return False to indicate we couldn't open the shm in the first place

    FunctionShm *shm = mmap(NULL, FUNCTION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shm == MAP_FAILED) return Py_False;

    // Lock the mutex
    pthread_mutex_lock(&(shm->mutex));

    // Set the activity byte to 0 to indicate inactive
    shm->activity = 0;

    // Signal the function to have it read inactivity and shut down
    pthread_cond_signal(&(shm->fcond));
    pthread_mutex_unlock(&(shm->mutex));
    return Py_True; // Return True to indicate success
}

static PyMethodDef methods[] = {
    {"create_memory", (PyCFunction)create_memory, METH_VARARGS | METH_KEYWORDS, "Create a shared memory address."},
    {"remove_memory", (PyCFunction)remove_memory, METH_VARARGS | METH_KEYWORDS, "Remove a shared memory address."},
    {"read_memory", read_memory, METH_VARARGS, "Get the value stored in a shared memory address."},
    {"write_memory", (PyCFunction)write_memory, METH_VARARGS | METH_KEYWORDS, "Write a value to a shared memory address."},

    {"create_function", create_function, METH_VARARGS, "Create and link a function to shared memory."},
    {"remove_function", remove_function, METH_VARARGS, "Stop a function linked to shared memory."},
    {"call_function", call_function, METH_VARARGS, "Call a function linked to shared memory."},

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
    "A module for managing shared memory with Python.",
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

