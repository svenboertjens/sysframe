# pybytes

A module for converting values to bytes and vice versa.


## Methods

There's currently only methods for serializing and de-serializing values, though there will be other methods with informative purposes soon. With informatic, I mean stuff like testing whether the datatype of a value is supported, the protocol of a bytes object, or whether a bytes object is valid for pybytes.
Even though there might be newer protocols in the future, it will still be possible to pass byte objects built by older protocols. This is all handled within the regular conversion functions.


### Conversion methods:

To bytes:   `from_value(value: any) -> bytes`
From bytes: `to_value(bytes_obj: bytes) -> any`

It supports the following datatypes:

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


An example on how to use these functions:
```
from sysframe import pybytes

# Our original value, which we want to convert
original_value = [3.142, 'Hello, world!', True]

# Convert it to bytes using the 'from_value' function
bytes_obj = pybytes.from_value(original_value)

# And convert it back to the original value
reconstructed_value = pybytes.to_value(bytes_obj)
```

