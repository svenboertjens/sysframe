# membridge

A module for managing shared memory with Python.


## Methods

This module offers methods for a couple of things related to IPC operations. These operations being:

Basic shared memory:
It offers the basic shared memory operations which allow storage and retrieval of a shared memory address.

IPC function calls:
It offers operations to create shared functions, which are functions defined by one process and callable by other processes, letting them run in the context of the process that defined the function.


### Basic shared memory:

Create: `create_memory(name: str, prealloc_size: int=None, error_if_exists: bool=False) -> bool`
Remove: `remove_memory(name: str, throw_error: bool=False) -> bool`
Read:   `read_memory(name: str) -> any`
Write:  `write_memory(name: str, value: any, create: bool=True) -> bool`

It's not necessary to define the `prealloc_size` when creating the shared memory, as the memory size is managed dynamically.

Here is an example on using these functions:

```
# store_value.py

from sysframe import membridge

# This is the value we'd like to share with other processes
value = ['Hello, other processes!', 3.142]

# This is the address name we will write it to
name = '/unique-example-name-abc'

# We can create it like this if we want, but that's not necessary
# It's automatically created if it doesn't yet if you call `write_memory`
membridge.create_memory(name)

# Write the value to the shared memory
membridge.write_memory(name, value)
```
```
# retrieve_value.py

from sysframe import membridge

# This is the address name we used to share a value in the other process
name = '/unique-example-name-abc'

# Retrieve the value by reading the shared memory segment
value = membridge.read_memory(name)

# Remove the shared memory once you don't need it anymore
membridge.remove_memory(name)
```

### IPC function calls:

Create: `create_function(name: str, function: callable) -> None`
Remove: `remove_function(name: str) -> bool`
Call:   `call_function(name: str, args: tuple) -> any`

Only one function can be linked to a shared memory segment at the same time.

Here is an example on how to use IPC function calls:
```
# link_function.py

from sysframe import membridge

# This is the address name we will link our function to
name = '/unique-example-name-123'

# This is the function we want to link
number = 0
def add_to_number(to_add: float):
    global number
    number += to_add
    return number

# Link the function
membridge.create_function(name, add_to_number)
```
```
# call_function.py

from membridge import sysframe

# This is the name that we linked the function to earlier
name = '/unique-example-name-123'

# This is the number we would like to add by calling the function
to_add = 3.5

# We can call the function and get the value it returns, in this case the updated number
# We should send the arguments in a tuple
new_number = membridge.call_function(name, (to_add,))

# Now, it's good practice to remove the function. We don't need it anymore
membridge.remove_function(name)

# And see here, the new number!
print(new_number)
```

