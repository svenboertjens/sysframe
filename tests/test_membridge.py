# Use the test values from `test_pybytes`
from test_pybytes import test_values
from sysframe import membridge

# The shared memory name
name = '/test-python-membridge-123'

# Create the shared memory
membridge.create_memory(name)

# Number of errors
errors = 0

# Go over the test values and read/write them
for value in test_values:
    write = membridge.write_memory(name, value)
    result = membridge.read_memory(name)
    
    if write == False: # Check if we couldn,t write
        print(f'Failed to write value {value}')
        errors += 1
    elif result == False and value is not False: # Check if we couldn't read
        print(f'Failed to read value {value}')
        errors += 1

# Close the shared memory
membridge.remove_memory(name)

# Print if there were no errors, or how many there were
print(errors == 0 and 'No errors' or f'{errors} errors')

