# Sysframe

A collection of modules useful for low-level development tasks.


## Introduction

This is a custom framework that provides the tools used by the various system services made by me. This includes modules such as for managing shared memory and other IPC operations, and serialization for converting values to bytes to store them in said shared memory.

This module is essentially a package providing tools for


## Current modules

This framework currently consists of the following modules:

- `pybytes`
- `membridge`

Usage explanations on these modules can be found under the `documentation` directory.


## Modules

Beside the module-specific explanations, here's a brief mention of each module and their functionality.


### Pybytes

Pybytes is a module that offers lossless serialization methods that aim to keep the serialized objects byte size small, while also maintaining fast and practically neglectible serialization speeds.
It aims to cover a wide variety of core Python datatypes. Here's a list of all (currently) supported datatypes:

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
  - `timedelta`
  - `datetime`
  - `date`
  - `time`
- `uuid.UUID`
- `memoryview`
- `decimal.Decimal`
- `range`
- `collections`:
  - `deque`
  - `namedtuple`
  - `Counter`
  - `OrderedDict`

If you need support for additional datatypes, feel free to request them!


### Membridge

Membridge is an IPC module that aims to simplify the sometimes complex usage of shared memory for you.
It automatically manages the shared memory size for you and (de-)serializes everything internally, so that you only have to input the value you want to store, and can retrieve it as well, without having to use a serializer anywhere.


## Contact

If you happen to find a problem or have a question or suggestion, feel free to contact me via:

- Discord: `sven_de_pen`,
- Email:   `boertjens.sven@gmail.com`

Alternatively, you can create an issue or start a discussion on the GitHub repository:

[sysframe github](https://github.com/svenboertjens/sysframe)

