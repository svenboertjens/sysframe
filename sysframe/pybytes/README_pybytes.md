# pybytes

A module for converting values to bytes and vice versa.


## Methods

There's two methods for converting stuff to bytes and back.
One is more generic and accepts most basic values, and the other is more direct, but requires you to input what datatype it is.


### The generic method:

To bytes:   `from_value(value: any) -> bytes`
From bytes: `to_value(bytes_obj: bytes) -> any`

Simple enough, right? Here's an example on how to use it:
```
from sysframe import pybytes

# Our original value, which we want to convert
original_value = [3.142, 'Hello, world!', True]

# Convert it to bytes using the 'from_value' function
bytes_obj = pybytes.from_value(original_value)

# And convert it back to the original value
reconstructed_value = pybytes.to_value(bytes_obj)
```


### The direct method:

To bytes:   `from_single_value(value: any, datatype: str) -> bytes`
From bytes: `to_single_value(bytes_obj: bytes, datatype: str) -> any`


As you can see, it requires the datatype as input (as a string, not type).
This method can be used when you know what datatype to expect, and it allows for generally faster conversion speeds and a slightly lower byte size. This method doesn't work for iterable or dict types.


An example with this method:
```
from sysframe import pybytes

# Let's say we want to convert the input of the user
original_value = input()

# We know that this is a string value, so we can use the direct method
bytes_obj = pybytes.from_single_value(original_value, 'str')

# Now, we convert it back to what it used to be again
reconstructed_value = pybytes.to_single_value(bytes_obj, 'str')
```

* Note: The `datatype` argument of this method relies on the first character of the datatype, meaning that for ease of use, you can input just `'s'` instead of `'str'`, or `'i'` instead of `'int'`, etc.

