# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys

from setuptools import setup, find_packages

here = os.path.abspath(os.path.dirname(__file__))
if here not in sys.path:
    sys.path.insert(0, here)

import json5

with open(os.path.join(here, 'README.md')) as fp:
    long_description = fp.read()


setup(
    name='json5',
    packages=find_packages(exclude=['tests']),
    entry_points={
        'console_scripts': [
            'pyjson5=json5.tool:main',
        ]
    },
    install_requires=[
    ],
    extras_require={
        'dev': [
            'hypothesis'
        ]
    },
    version=json5.VERSION,
    author='Dirk Pranke',
    author_email='dpranke@chromium.org',
    description=long_description.splitlines()[2],
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/dpranke/pyjson5',
    license='Apache',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
    ],
)
