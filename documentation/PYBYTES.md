# pybytes

A module for serializing variables.


## Methods

- Serialize:    `from_value(value: any) -> bytes`
- De-serialize: `to_value(bytes_obj: bytes) -> any`

The supported datatypes are listed in the global README.


An example on how to use these methods:
```
from sysframe import pybytes

# Our original value, which we want to convert
original_value = [3.142, 'Hello, world!', True]

# Convert it to bytes using the 'from_value' function
bytes_obj = pybytes.from_value(original_value)

# And convert it back to the original value
reconstructed_value = pybytes.to_value(bytes_obj)
```

