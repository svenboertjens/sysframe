# membridge.pyi

def create_memory(name: str, prealloc_size: int=None, error_if_exists: bool=False) -> bool:
    """
    Create a shared memory segment.
    
    This function does not have to be called, as it will be called automatically when writing memory to a shared memory address (unless specified otherwise).
    
    Arguments:
    - `name`: The unique name for your shared memory, used to be able to locate it in other processes as well.
    - `prealloc_size`: Space to allocate up front for data to write to (optional).
    - `error_if_exists`: Throw an error if the shared memory address already exists (optional).
    
    The `prealloc_size` argument is optional because the shared memory will resize automatically based on what is written to it.
    
    If `error_if_exists` is set to `False`, it will return `False` on failure.
    
    """
    ...

def remove_memory(name: str, throw_error: bool=False) -> bool:
    """
    Remove a shared memory segment.
    
    This function should be called if you no longer want to use the shared memory, so that it can be cleaned up properly.
    
    Arguments:
    - `name`: The unique name of the shared memory segment you want to remove.
    - `throw_error`: Throw an error if the shared memory can't be removed, such as if it's already removed (optional).
    
    If `throw_error` is set to `False`, it will return `False` on failure.
    
    """
    ...

def read_memory(name: str) -> any:
    """
    Read the value stored to a shared memory segment.
    
    Arguments:
    - `name`: The unique name of the shared memory segment you want to read the value from.
    
    """
    ...

def write_memory(name: str, value: any, create: bool=True) -> bool:
    """
    Write a value to a shared memory segment.
    
    Arguments:
    - `name`: The unique name for your shared memory to write the value to.
    - `value`: The value you want to write to the shared memory.
    - `create`: Create the shared memory if it doesn't exist yet (optional).
    
    """
    ...

def create_function(name: str, function: callable) -> None:
    """
    Create and link a function to shared memory.
    
    Arguments:
    - `name`: The unique name for your shared memory.
    - `function`: The function (or any callable) you want to link to the shared memory.
    
    The given function will run in the context of the process that linked it to the shared memory.
    
    """
    ...

def remove_function(name: str) -> bool:
    """
    Stop a function linked to shared memory.
    
    Arguments:
    - `name`: The unique memory address name you want to remove the function from.
    
    This will stop the function from listening to calls, and close the shared memory segment.
    This will return True on success, and False if the shared memory segment couldn't be opened.
    
    """
    ...

def call_function(name: str, args: tuple) -> any:
    """
    Call a function linked to shared memory.
    
    Arguments:
    - `name`: The unique name that the function is linked to.
    - `args`: The arguments you want to send to the function
    
    This will call the linked function in the context the process that defined it.
    This will return the arguments sent by the linked function.
    
    """
    ...

