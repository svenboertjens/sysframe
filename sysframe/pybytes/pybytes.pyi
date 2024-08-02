# pybytes.pyi

def from_value(value: any) -> bytes:
    """
    Convert any value to a bytes object.
    
    Example usage:
    
    >>> # The value we want to convert to bytes
    >>> value = 'Hello, world!'
    >>> # Convert it to bytes using sysframe.pybytes.from_value()
    >>> bytes_obj = pybytes.from_value(value)
    
    All supported datatypes:
    - `str`
    - `int`
    - `float`
    - `bool`
    - `complex`
    - `NoneType`
    - `ellipsis`
    - `bytes`
    - `bytearray`
    - `datetime`:
        * `timedelta`
        * `datetime`
        * `date`
        * `time`
    - `uuid.UUID`
    - `memoryview`
    - `decimal.Decimal`
    - `range`
    - `collections`:
        * `deque`
        * `namedtuple`
        * `Counter`
        * `OrderedDict`
    
    Convert the value back using `pybytes.to_value`
    """
    ...

def to_value(bytes_obj: bytes) -> any:
    """
    Convert a bytes object created by `pybytes.from_value` back to its original value.
    
    Example usage:
    
    >>> # The bytes object we got from `pybytes.from_value`
    >>> bytes_obj = b'...'
    >>> # Convert it back to the original value
    >>> value = pybytes.to_value(bytes_obj)
    """
    ...

