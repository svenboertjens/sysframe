# membridge

A module for using shared memory tools in Python.


## Methods

This module offers methods for a couple of things related to IPC operations. These operations being:

Basic shared memory:
It offers the basic shared memory operations which allow storage and retrieval of a shared memory address.

IPC function calls:
It offers operations to create shared functions, which are functions defined by one process and callable by other processes, letting them run in the context of the process that defined the function.


### Basic shared memory:

Create: `create_memory(memory_name: str) -> bool`
Remove: `remove_memory(memory_name: str) -> bool`
Read:   `read_memory(memory_name: str) -> bytes`
Write:  `write_memory(memory_name: str, value: any) -> bool`

There's no need to define a size anywhere, that part is managed internally.
Here's an example on how to use these functions:
```
# store_value.py

from sysframe import membridge

# Imagine, we have a variable we'd like to share
value = ['Hello, other processes!', 3.142]

# This is the memory address name we will use
name = '/unique-example-name'

# We have to create the shared memory address to ensure it exists
membridge.create_memory(name)

# Now, we write the value to it, making it accessible to other processes
membridge.write_memory(name, value)
```
```
# retrieve_value.py

from sysframe import membridge

# In this other process, we want to read what value was stored on this address
name = '/unique-example-name-abc'

# We can retrieve the value like this
value = membridge.read_memory(name)

# Now, we want to close the shared memory to avoid memory leaks
membridge.remove_memory(name)
```

### IPC function calls:

Create: `create_function(memory_name: str, function: callable) -> bool`
Remove: `remove_function(memory_name: str) -> int`
Call:   `call_function(memory_name: str, args: tuple) -> any`

Here, you can create a shared memory with a function linked to it, which can be called by other processes.

An example on how this all works:
```
# link_function.py

from sysframe import membridge

# This is the function we want to link to the shared memory
def ipc_add_nums(num1, num2):
    # Return the numbers multiplied by each other
    return num1 * num2

# This is the name of the shared memory address
name = '/unique-example-name-123'

# Set up the shared function and get the status of the call, which is False on failures and True on non-error terminations
status = membridge.create_function(name, ipc_add_nums)

# You can do whatever to handle success/failure as the termination reason
if status == True:
    print('Function was terminated by another process!')
else:
    print('Function was terminated due to an error!')
```
```
# call_function.py

from sysframe import membridge

# The name of the shared memory address, as defined in the other script
name = '/unique-example-name-123'

# These are the numbers we want to multiply
num1 = 9
num2 = 13

# Add them into a tuple to send them along
args = (
    num1,
    num2
)

# Call the function and get the return value it sends back
result = membridge.call_function(name, args)

# And now, we have what 9 * 13 is!
print(result)

# Now, it's good practice to shut down the shared function because we no longer need it
membridge.remove_function(name)

# This remove function returns 1 on successful removal, 2 if it can't be found in the first place (already removed, for example)
# and 3 if the function is successfully removed, but the shared memory address itself couldn't be removed.
```

