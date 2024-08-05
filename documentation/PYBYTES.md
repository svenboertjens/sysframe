# pybytes

A module for serialization and data storage.


## Methods

Currently, only the serialization methods are available. The storage methods are still in heavy development.


### Serialization:

Serialize:    `from_value(value: any) -> bytes`
De-serialize: `to_value(bytes_obj: bytes) -> any`

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

### Storage:

This is still in very early stages (and thus unreleased), so there's not much to say here currently.

