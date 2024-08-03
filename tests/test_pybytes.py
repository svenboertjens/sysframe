from unittest import TestCase, main
import pybytes

from collections import *
from pathlib import Path, PurePath
import datetime
import decimal
import uuid


# A list with all values to test
test_values = [
    # Str
    'Hello, world!',
    '',
    'Hello, world!' * 100000,
    '\t\n!@#$%^&*()~`_+-=[]{}|",./<>?' * 100000,
    # Int
    12345,
    10**1000,
    -(10**1000),
    0,
    # Float
    3.142,
    0.0,
    # Bool
    True,
    False,
    # NoneType
    None,
    # Complex
    2j + 3,
    0.000001j - 9999999,
    # Ellipsis
    ...,
    # Bytes
    b'Hello, world!',
    b'',
    b'Hello, world!' * 100000,
    b'\t\n!@#$%^&*()~`_+-=[]{}|",./<>?' * 100000,
    # Bytearray
    bytearray(b'Hello, world!'),
    bytearray(b''),
    bytearray(b'Hello, world!' * 100000),
    bytearray(b'\t\n!@#$%^&*()~`_+-=[]{}|",./<>?' * 100000),
    # Datetime
    datetime.datetime(2008, 6, 8, 23, 53),
    datetime.datetime(9999, 12, 31, 23, 59, 59, 999),
    datetime.timedelta(5, 14, 12, 11, 43, 19, 2),
    datetime.timedelta(6, 59, 999, 999, 59, 23, 51),
    datetime.date(2008, 6, 8),
    datetime.date(9999, 12, 31),
    datetime.time(23, 53),
    datetime.time(23, 59, 59, 999),
    # Decimal
    decimal.Decimal('3.1415926'),
    decimal.Decimal('3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679'),
    # UUID
    uuid.uuid1(),
    uuid.uuid4(),
    # Memoryview
    memoryview(b'Hello, world!'),
    memoryview(b'Hello, world!' * 100000),
    memoryview(b'\t\n!@#$%^&*()~`_+-=[]{}|",./<>?' * 100000),
    # Range
    range(0, 100, 2),
    range(-1000000000000, 1000000000000, 1000000000),
    # Namedtuple
    namedtuple('awesome_namedtuple', ['some', 'interesting', 'values'])('with', 'interesting', 'items'),
    namedtuple('_', [])(),
    namedtuple('hello', ['world'])(namedtuple('banana', ['woah'])('some_value')),
    # Deque
    deque([1, 2, 3, 4, 5]),
    deque([]),
    deque([deque([1, 2, 3, deque([4, 5, 6])])]),
    # Counter
    Counter('abcdeabcdabcaba'),
    Counter(),
    # OrderedDict
    OrderedDict({'Hello': 'world!', 'some': 'key', 'value': 'pairs'}),
    OrderedDict(),
    OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict())))))),
    # ChainMap
    ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4}),
    ChainMap(ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4}), ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4})),
    # List
    [3.142, None, 'Hello, world!'],
    [],
    [[[[['Hello,', [[[]]], 'world!']]]]],
    # Dict
    {3.142: 'Hello, world!', True: False},
    dict(),
    {'Hello,': {'world!': {'This': {'is': {'deeply': {'nested!': {}}}}}}},
    # Tuple
    (9009, 'banananana'),
    tuple(),
    (((((((('All those commas...',),),),),),),),),
    # Set
    {'What is your favorite music genre?'},
    set(),
    # Frozenset
    frozenset([3.142, None, 'Hello, world!']),
    frozenset(),
    # Path
    Path(),
    Path('/home/usr2/Pictures'),
    # PurePath
    PurePath(),
    PurePath('/home/usr2/Downloads'),
]


class TestPybytes(TestCase):
    # Helper function to test encoding/decoding
    def assertFromTo(self, value):
        try:
            bytes_obj = pybytes.from_value(value)
            decoded = pybytes.to_value(bytes_obj)
            self.assertEqual(value, decoded, f"Failed for value: {value}")
        except:
            self.fail(f"Error with value: {value}")
    
    def test_values(self):
        # Test each value separately
        for value in test_values:
            self.assertFromTo(value)
        
        # Test the whole list with testing values as well
        self.assertFromTo(test_values)

if __name__ == '__main__':
    main()

