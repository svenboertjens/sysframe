# Sysframe

A framework designed for providing tools to develop system services with.


## Introduction

This is a custom framework that provides the tools used by the various system services made by me. This includes modules such as for managing shared memory and other IPC operations, and serialization for converting values to bytes to store them in said shared memory.


## Current modules

This framework currently consists of the following modules:

- `pybytes`
- `membridge`

Explanations on how to use these modules are found in module-specific README files, such as `README_pybytes.md` for the `pybytes` module.
For code-specific documentation, there aren't special README's for that. I've tried to explain each piece of code well enough using in-code comments.


## Modules

Here, you can find a brief explanation of each module contained by this package.
For detailed explanation per module, you can find a module-specific README in the module dir.


### Pybytes

Pybytes is a module that's used for serializing and de-serializing values. It aims to keep the byte size smaller and excels at serializing smaller values as well, making it suitable for system development.

It supports the following datatypes:

- `str`
- `int`
- `float`
- `bool`
- `complex` (the number)
- `NoneType`
- `ellipsis`
- `bytes`
- `bytearray`
- `datetime`:
    * `timedelta` (not yet, but hopefully soon)
    * `datetime`
    * `date`
    * `time`
- `uuid.UUID`
- `memoryview`
- `decimal.Decimal`

Besides that, it also supports any of the standard list types, those being `list`, `dict`, `tuple`, `set` and `frozenset`. These are also allowed to be nested to (theoretically) unlimited depth.


### Membridge

Membridge is a module for IPC operations, allowing independent processes to communicate. This was added to the sysframe package because it supports shared function calls, which can be used to keep the 'main' process of a service the actual service, and have it provide handles to other processes so that they can operate using the main process. This is useful if you want to avoid having multiple processes running the same operations, which can be considered a waste of resources.

Membridge uses the `sysframe.pybytes` module for serializing and de-serializing the values to be passed to the shared memory. If you want to use a different serializer that would support other datatypes for example, you can use whichever serializer you want, and pass the received bytes object to membridge, as `sysframe.pybytes` supports byte objects.

This module has not been tested yet, and is still being worked on. Thus, it's also not in the package currently.


## Contact

If you happen to find a problem or have a question, feel free to contact me via:

Discord: `sven_de_pen`
Email:   `boertjens.sven@gmail.com`

