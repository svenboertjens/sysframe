from unittest import TestCase, main
import pybytes

from collections import *
import datetime
import decimal
import uuid


class TestPybytes(TestCase):
    # Helper function to test encoding/decoding
    def assertFromTo(self, value):
        bytes_obj = pybytes.from_value(value)
        decoded = pybytes.to_value(bytes_obj)
        self.assertEqual(value, decoded, f"Failed for value: {value}")
    
    # Test the supported 'standard' values regularly
    def test_standard_regular(self):
        # String
        self.assertFromTo('Hello, world!')
        
        # Integer
        self.assertFromTo(12345)
        
        # Float
        self.assertFromTo(3.142)
        
        # Bool (True)
        self.assertFromTo(True)
        
        # Bool (False)
        self.assertFromTo(False)
        
        # Complex
        self.assertFromTo(2j + 3)
        
        # None
        self.assertFromTo(None)
        
        # Ellipsis
        self.assertFromTo(...)
        
        # Bytes
        self.assertFromTo(b'Hello, world!')
        
        # Bytearray
        self.assertFromTo(bytearray(b'Hello, world!'))
    
    # Test the supported 'standard' values with edge cases
    def test_standard_edgecases(self):
        # String (large)
        self.assertFromTo('Hello, world!' * 100000)
        
        # String (special characters)
        self.assertFromTo('\t\n!@#$%^&*()~`_+-=[]{}|",./<>?')
        
        # Integer (large)
        self.assertFromTo(10**1000)
        
        # Float
        self.assertFromTo(-0.0)
        
        # Complex
        self.assertFromTo(0.000001j - 9999999)
        
        # Bytes (large)
        self.assertFromTo(b'Hello, world!' * 100000)
        
        # Bytes (special characters)
        self.assertFromTo(b'\t\n!@#$%^&*()~`_+-=[]{}|",./<>?')
        
        # Bytearray (large)
        self.assertFromTo(bytearray(b'Hello, world!' * 100000))
        
        # Bytearray (special characters)
        self.assertFromTo(bytearray(b'\t\n!@#$%^&*()~`_+-=[]{}",./<>?'))
    
    # Test the supported 'miscellaneous' (non-standard) values regularly
    def test_misc_regular(self):
        # Datetime (datetime)
        self.assertFromTo(datetime.datetime(2008, 6, 8, 23, 53))
        
        # Datetime (timedelta)
        self.assertFromTo(datetime.timedelta(5, 14, 12, 11, 43, 19, 2))
        
        # Datetime (date)
        self.assertFromTo(datetime.date(2008, 6, 8))
        
        # Datetime (time)
        self.assertFromTo(datetime.time(23, 53))
        
        # Decimal
        self.assertFromTo(decimal.Decimal('3.1415926'))
        
        # UUID (1)
        self.assertFromTo(uuid.uuid1())
        
        # UUID (4)
        self.assertFromTo(uuid.uuid4())
        
        # Memoryview
        self.assertFromTo(memoryview(b'Hello, world!'))
        
        # Range
        self.assertFromTo(range(0, 100, 2))
        
        # Namedtuple
        self.assertFromTo(namedtuple('awesome_namedtuple', ['some', 'interesting', 'values'])('with', 'interesting', 'items'))
        
        # Deque
        self.assertFromTo([1, 2, 3, 4, 5]);
        
        # Counter
        self.assertFromTo(Counter('abcdeabcdabcaba'))
        
        # OrderedDict
        self.assertFromTo(OrderedDict({'Hello': 'world!', 'some': 'key', 'value': 'pairs'}))
        
        # ChainMap
        self.assertFromTo(ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4}))
    
    # Test the supported 'miscellaneous' (non-standard) values with edge cases
    def test_misc_edgecases(self):
        # Datetime (datetime)
        self.assertFromTo(datetime.datetime(9999, 12, 31, 23, 59, 59, 999))
        
        # Datetime (timedelta)
        self.assertFromTo(datetime.timedelta(6, 59, 999, 999, 59, 23, 51))
        
        # Datetime (date)
        self.assertFromTo(datetime.date(9999, 12, 31))
        
        # Datetime (time)
        self.assertFromTo(datetime.time(23, 59, 59, 999))
        
        # Decimal (large)
        self.assertFromTo(decimal.Decimal('3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679'))
        
        # Memoryview (large)
        self.assertFromTo(memoryview(b'Hello, world!' * 1000))
        
        # Memoryview (special characters)
        self.assertFromTo(memoryview(b'\t\n!@#$%^&*()~`_+-=[]{}|",./<>?' * 100000))
        
        # Range
        self.assertFromTo(range(-1000000000000, 1000000000000, 1000000000))
        
        # Namedtuple (empty)
        self.assertFromTo(namedtuple('name_cant_be_empty', [])())
        
        # Namedtuple (nested)
        self.assertFromTo(namedtuple('hello', ['world'])(namedtuple('banana', ['woah'])('some_value')))
        
        # Deque (empty)
        self.assertFromTo([]);
        
        # Deque (nested)
        self.assertFromTo([deque([1, 2, 3, deque([4, 5, 6])])]);
        
        # Counter (empty)
        self.assertFromTo(Counter())
        
        # OrderedDict (empty)
        self.assertFromTo(OrderedDict())
        
        # OrderedDict (nested)
        self.assertFromTo(OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict(OrderedDict())))))))
        
        # ChainMap (empty)
        self.assertFromTo(ChainMap())
        
        # ChainMap (nested)
        self.assertFromTo(ChainMap(ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4}), ChainMap({'a': 2, 'b': 3}, {'b': 1, 'c': 4})))
    
    # Test the supported list types regularly
    def test_list_types_regular(self):
        # List
        self.assertFromTo([3.142, None, 'Hello, world!'])
        
        # dict
        self.assertFromTo({3.142: 'Hello, world!', True: False})
        
        # Tuple
        self.assertFromTo((9009, 'banananana'))
        
        # Set
        self.assertFromTo({'What is your favorite music genre?'})
        
        # Frozenset
        self.assertFromTo(frozenset([3.142, None, 'Hello, world!']))
    
    # Test the supported list types with edge cases
    def test_list_types_edgecases(self):
        # List (empty)
        self.assertFromTo([])
        
        # List (nested)
        self.assertFromTo([[[[['Hello,', [[[]]], 'world!']]]]])
        
        # dict (empty)
        self.assertFromTo(dict())
        
        # dict (nested)
        self.assertFromTo({'Hello,': {'world!': {'This': {'is': {'deeply': {'nested!': {}}}}}}})
        
        # Tuple (empty)
        self.assertFromTo(tuple())
        
        # Tuple (nested)
        self.assertFromTo((((((((('All those commas...',),),),),),),),))
        
        # Set (empty)
        self.assertFromTo(set())
        
        # Frozenset (empty)
        self.assertFromTo(frozenset())


if __name__ == '__main__':
    main()

