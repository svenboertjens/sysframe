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

typedef struct
{
    pthread_mutex_t mutex;
    void *value;
} SharedMemoryStruct;

#define SHM_SIZE sizeof(SharedMemoryStruct)

PyObject *create_shared_memory(const char *name)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return Py_False;
    }

    if (ftruncate(fd, SHM_SIZE) == -1)
    {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return Py_False;
    }

    SharedMemoryStruct *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        return Py_False;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm_ptr->mutex, &attr);
    shm_ptr->value = NULL;

    munmap(shm_ptr, SHM_SIZE);
    close(fd);

    return Py_True;
}

PyObject *create_memory(PyObject *self, PyObject *args)
{
    PyObject *name;

    if (!PyArg_ParseTuple(args, "O!", &PyUnicode_Type, &name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    // Create the shared memory and return the result
    return create_shared_memory(PyUnicode_AsUTF8(name));
}

PyObject *remove_memory(PyObject *self, PyObject *args)
{
    PyObject *py_name;

    if (!PyArg_ParseTuple(args, "O!", &PyUnicode_Type, &py_name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    // Convert the name to a C char
    char *name = PyUnicode_AsUTF8(py_name);

    // Unlink the memory
    if (!shm_unlink(name))
    {
        // Failed to unlink, return False to indicate failure
        return Py_False;
    }

    // Return True to indicate success
    return Py_True;
}

char *read_shared_memory(const char *name)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return NULL;
    }

    SharedMemoryStruct *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return NULL;
    }

    pthread_mutex_lock(&shm_ptr->mutex);
    char *value = shm_ptr->value;
    pthread_mutex_unlock(&shm_ptr->mutex);

    munmap(shm_ptr, SHM_SIZE);
    close(fd);

    return value;
}

PyObject *read_memory(PyObject *self, PyObject *args)
{
    PyObject *name;

    if (!PyArg_ParseTuple(args, "O!", &PyUnicode_Type, &name))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'str' type.");
        return NULL;
    }

    // Fetch the value stored in the shared memory
    char *value = read_shared_memory(PyUnicode_AsUTF8(name));

    // Return the value as a Python bytes object
    return PyBytes_FromString(value);
}

void write_shared_memory(const char *name, char *value)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to open the shared memory address.");
        return NULL;
    }

    SharedMemoryStruct *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory address.");
        return NULL;
    }

    pthread_mutex_lock(&shm_ptr->mutex);
    shm_ptr->value = value;
    pthread_mutex_unlock(&shm_ptr->mutex);

    munmap(shm_ptr, SHM_SIZE);
    close(fd);
}

PyObject *write_memory(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *value;

    if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &name, &PyBytes_Type, &value))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'bytes' type.");
        return NULL;
    }

    // Write it to the shared memory address
    write_shared_memory(PyUnicode_AsUTF8(name), PyBytes_AsString(value));

    // Return True to indicate success
    return Py_True;
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t func_cond;
    pthread_cond_t call_cond;
    char activity;
    void *args;
} shared_function;

// Initiate a shared memory for a shared function
PyObject *create_shared_function(const char *name, PyObject *func)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        // Check whether it already exists or if it's a different error
        if (errno == ENOENT)
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
    pthread_condattr_t cond_attr;

    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(func_cond, &cond_attr);

    pthread_cond_t *call_cond = &(data->call_cond);
    pthread_condattr_t cond_attr;

    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(call_cond, &cond_attr);

    // Set the activity byte to \x00 to indicate it's active
    data->activity = '\x00';

    // Set a while loop to repeatedly catch all signals
    while (1)
    {
        // Begin waiting on the cond to be signaled
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&func_cond, &mutex);

        // Check if the activity byte is set to inactive
        if (data->activity == '\x01')
        {
            // Break the loop to do cleanup
            break;
        }

        // Call the function and parse the args to it
        PyObject *return_args = PyObject_CallFunction(func, "O", (PyObject *)(data->args));

        Py_INCREF(return_args);

        // Set the args to the returned args so that the caller can fetch them
        data->args = return_args;

        // Signal the caller that the returned args are set
        pthread_cond_signal(&call_cond);

        // Unlock the mutex after performing the function call to prevent signals from missing this loop
        pthread_mutex_unlock(&mutex);
    }

    // Cleanup for when the loop breaks
    munmap(shm_ptr, sizeof(shared_function));
    close(fd);

    // Return True to indicate we didn't exit due to a failure
    return Py_True;
}

PyObject *create_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *func;

    if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &name, &PyFunction_Type, &func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'function' type.");
        return NULL;
    }

    // Call the function to create the shared function
    return create_shared_function(PyUnicode_AsUTF8(name), func);
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

    // Assign the args to the shared memory
    data->args = (void *)args;

    // Signal the halted thread
    pthread_cond_signal(&(data->func_cond));
    // Wait on the caller signal to fetch the returned args
    pthread_cond_wait(&(data->call_cond), &(data->mutex));

    // Get the returned args as a Python object
    PyObject *returned_args = (PyObject *)(data->args);

    pthread_mutex_unlock(&(data->mutex));
    close(fd);

    // Return the returned args from the called function
    return returned_args;
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

    // Call the shared function and get the return state
    PyObject *return_state = call_shared_function(PyUnicode_AsUTF8(name), py_args);

    Py_DECREF(name);
    Py_DECREF(py_args);

    // Return the return state to the user
    return return_state;
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

    // Set the activity byte to \x01 to indicate inactive
    data->activity = '\x01';

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
    {"create_memory", create_memory, METH_VARARGS, "Create a shared memory address."},
    {"remove_memory", remove_memory, METH_VARARGS, "Remove a shared memory address."},
    {"read_memory", read_memory, METH_VARARGS, "Get the value stored in a shared memory address."},
    {"write_memory", write_memory, METH_VARARGS, "Write a value to a shared memory address."},

    {"create_function", create_function, METH_VARARGS, "Create and link a shared function handle."},
    {"remove_function", remove_function, METH_VARARGS, "Unlink a shared function and its memory address."},
    {"call_function", call_function, METH_VARARGS, "Call a function linked to a shared function handle."},

    {NULL, NULL, 0, NULL}
};

void membridge_module_cleanup(void *module)
{
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
    Py_Initialize();
    return PyModule_Create(&membridge);
}

