from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext as build_ext_orig
import unittest

class build_ext(build_ext_orig):
    def build_extensions(self):
        ct = self.compiler.compiler_type
        if ct == 'unix':
            for ext in self.extensions:
                ext.extra_compile_args = ['-O3']
        super().build_extensions()

with open("README.md", "r") as file:
    long_description = file.read()

setup(
    name="sysframe",
    version="0.3.0",
    
    author="Sven Boertjens",
    author_email="boertjens.sven@gmail.com",
    
    description="A collection of modules useful for low-level development tasks.",
    
    long_description=long_description,
    long_description_content_type="text/markdown",
    
    url="https://github.com/svenboertjens/sysframe",
    
    packages=find_packages(),
    include_package_data=True,
    install_requires=[],
    package_data={
        '': ['*.pyi'],
    },
    
    ext_modules=[
        Extension( # Pybytes
            
            'sysframe.pybytes.pybytes',
            sources=[
                'sysframe/pybytes/pybytes.c',
                'sysframe/pybytes/sbs_main/sbs_2.c',
                'sysframe/pybytes/sbs_old/sbs_1.c',
            ],
            include_dirs=[
                'sysframe/pybytes',
            ]
        ),
        Extension( # membridge
        
            'sysframe.membridge.membridge',
            sources=[
                'sysframe/membridge/membridge.c',
                'sysframe/pybytes/sbs_main/sbs_2.c',
                'sysframe/pybytes/sbs_old/sbs_1.c',
            ],
            include_dirs=[
                'sysframe/membridge',
                'sysframe/pybytes',
            ]
        )
    ],
    
    test_suite='tests',
    tests_require=['unittest'],
    
    cmdclass={'build_ext': build_ext},
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: Linux",
    ],
    python_requires='>=3.6, <3.13',
)