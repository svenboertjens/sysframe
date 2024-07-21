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
#include "pybytes.h"

typedef struct
{
    pthread_mutex_t mutex;
    void *value;
} SharedMemoryStruct;

#define SHM_SIZE sizeof(SharedMemoryStruct)

void create_shared_memory(const char *name)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return;
    }

    if (ftruncate(fd, SHM_SIZE) == -1)
    {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return;
    }

    SharedMemoryStruct *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        return;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm_ptr->mutex, &attr);
    shm_ptr->value = NULL;

    munmap(shm_ptr, SHM_SIZE);
    close(fd);
}

void remove_shared_memory(const char *name)
{
    shm_unlink(name);
}

void *read_shared_memory(const char *name)
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
    void *value = shm_ptr->value;
    pthread_mutex_unlock(&shm_ptr->mutex);

    munmap(shm_ptr, SHM_SIZE);
    close(fd);

    return value;
}

void write_shared_memory(const char *name, void *value)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return;
    }

    SharedMemoryStruct *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return;
    }

    pthread_mutex_lock(&shm_ptr->mutex);
    shm_ptr->value = value;
    pthread_mutex_unlock(&shm_ptr->mutex);

    munmap(shm_ptr, SHM_SIZE);
    close(fd);
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    size_t size;
} shared_function;

// Initiate a shared memory for a shared function
PyObject *create_shared_function(const char *name, size_t size)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to create the shared memory.");
        return NULL;
    }

    // Calculate the total size, which is the metadata plus the arg size
    size_t total_size = sizeof(shared_function) + size;

    if (ftruncate(fd, total_size) == -1)
    {
        close(fd);
        shm_unlink(name);
        PyErr_SetString(PyExc_MemoryError, "Failed to set up the shared memory.");
        return NULL;
    }

    void *shm_ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        close(fd);
        shm_unlink(name);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Create the shared function struct
    shared_function *metadata = shm_ptr;

    // Write the size of the args to the size attribute
    metadata->size = size;

    // Initialize mutex and condition variable
    pthread_mutex_t *mutex = &(metadata->mutex);
    pthread_mutexattr_t mutex_attr;

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(mutex, &mutex_attr);

    pthread_cond_t *cond = &(metadata->cond);
    pthread_condattr_t cond_attr;

    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(cond, &cond_attr);

    munmap(shm_ptr, total_size);
    close(fd);

    // Return a True object to indicate success
    return Py_True;
}

PyObject *create_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *size;

    if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &name, &PyLong_Type, &size))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'int' type.");
        return NULL;
    }

    // Call the function to create the shared function
    return create_shared_function(PyUnicode_AsUTF8(name), PyLong_AsSize_t(size));
}

typedef struct {
    PyObject *func;
    char *args;
} call_py_func_args;

void* call_python_function(void *args)
{
    // Convert it back to a struct
    call_py_func_args *python_args = (call_py_func_args *)args;
    // Call the function and parse the args converted from bytes to a tuple
    PyObject_CallFunction(python_args->func, "O", to_value(NULL, PyBytes_FromString(python_args->args)));
}

// Link a function to a shared memory conditional
PyObject *link_shared_function(const char *name, PyObject *func)
{
    // The size of the metadata info
    size_t meta_size = sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) + sizeof(size_t);

    int fd_meta = shm_open(name, O_RDWR, 0666);
    if (fd_meta == -1)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to open the shared memory.");
        return NULL;
    }

    void *shm_meta = mmap(NULL, meta_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_meta, 0);
    if (shm_meta == MAP_FAILED)
    {
        close(fd_meta);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Get the metadata of the shared memory
    shared_function *metadata = shm_meta;

    // Calculate the total size
    size_t total_size = sizeof(shared_function) + metadata->size;

    // Close the memory opened with just the metadata
    munmap(shm_meta, meta_size);
    close(fd_meta);

    // And open the full shared memory
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1)
    {
        PyErr_SetString(PyExc_MemoryError, "Failed to open the shared memory.");
        return NULL;
    }

    void *shm_ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        close(fd);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Continue waiting for calls until the loop is stopped
    while (1)
    {
        pthread_mutex_lock(&(metadata->mutex));
        pthread_cond_wait(&(metadata->cond), &(metadata->mutex));

        // Get the args in a string
        char args[metadata->size];
        // Copy the string from the offset after the metadata
        strncpy(args, shm_ptr + sizeof(shared_function), metadata->size);

        // Get a struct to store the function call arguments in
        call_py_func_args *python_args;
        // Assign values to it
        python_args->func = func;
        python_args->args = args;

        // Thread the function call to prevent halting the mutex for too long
        pthread_t thread;
        pthread_create(&thread, NULL, call_python_function, (void *)&python_args);

        // Unlock the mutex after threading the function call to prevent signals from missing this loop
        pthread_mutex_unlock(&(metadata->mutex));
    }

    munmap(shm_ptr, total_size);
    close(fd);

    // Return a True object to indicate success
    return Py_True;
}

PyObject *link_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *func;

    // Get the shared memory name and the function to link, and whether to thread it
    if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &name, &PyFunction_Type, &func))
    {
        PyErr_SetString(PyExc_ValueError, "Expected a 'str' and 'function' type.");
        return NULL;
    }

    // Call the function to link the parsed Python function to the shared memory signal
    return link_shared_function(PyUnicode_AsUTF8(name), func);
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

    void *shm_meta = mmap(NULL, sizeof(shared_function), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_meta == MAP_FAILED)
    {
        close(fd);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Get the metadata itself from the shm of the metadata
    shared_function *metadata = shm_meta;

    pthread_mutex_lock(&(metadata->mutex));

    // Calculate the total size of the shared memory block
    size_t total_size = sizeof(shared_function) + metadata->size;

    munmap(shm_meta, sizeof(shared_function));

    // Get the full shared memory now that we have the full size
    void *shm_ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_meta == MAP_FAILED)
    {
        close(fd);
        PyErr_SetString(PyExc_MemoryError, "Failed to map the shared memory.");
        return NULL;
    }

    // Convert the args to a pybytes object
    PyObject *py_result = from_value(NULL, args);
    // And convert it to a C string
    char *result = PyBytes_AsString(py_result);

    Py_DECREF(py_result);

    // Copy the concatenated string to the shared memory
    char *pointer_args = (char *)((char *)shm_ptr + sizeof(shared_function));
    memset(pointer_args, 0, total_size - sizeof(shared_function));            // Zero out the existing memory to prevent false reads on not overwritten bytes
    strcpy(pointer_args, result);

    // Signal the halted thread
    pthread_cond_signal(&(metadata->cond));
    pthread_mutex_unlock(&(metadata->mutex));
    close(fd);

    // Return a True object to indicate success
    return Py_True;
}

PyObject *call_function(PyObject *self, PyObject *args)
{
    PyObject *name;
    PyObject *tuple_args;

    if (PyArg_ParseTuple(args, "O!O", &PyUnicode_Type, &name, &tuple_args) == -1)
    {
        PyErr_SetString(PyExc_ValueError, "Expected 'str' and 'tuple' type.");
        return NULL;
    }

    if (!PyTuple_Check(tuple_args))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 'str' and 'tuple' type.");
        return NULL;
    }

    Py_INCREF(name);
    Py_INCREF(tuple_args);

    void *state = call_shared_function(PyUnicode_AsUTF8(name), tuple_args);

    Py_DECREF(name);
    Py_DECREF(tuple_args);

    return state;
}

static PyMethodDef methods[] = {
    {"create_function", create_function, METH_VARARGS, "Create a shared function handle."},
    {"link_function", link_function, METH_VARARGS, "Link a function to a shared function handle."},
    {"call_function", call_function, METH_VARARGS, "Call a function linked to a shared function handle."},
    {NULL, NULL, 0, NULL}
};

void module_cleanup(void *module)
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
    module_cleanup};

PyMODINIT_FUNC PyInit_membridge(void)
{
    Py_Initialize();
    return PyModule_Create(&membridge);
}

