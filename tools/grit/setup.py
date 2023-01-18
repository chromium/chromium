#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install the package!"""


import setuptools


setuptools.setup(
    name='grit',
    version='0',
    entry_points={
        'console_scripts': ['grit = grit.grit_runner:Main'],
    },
    packages=setuptools.find_packages(),
    author='The Chromium Authors',
    author_email='chromium-dev@chromium.org',
    description=('Google Resource and Internationalization Tool for managing '
                 'translations & resource files'),
    license='BSD-3',
    url='https://chromium.googlesource.com/chromium/src/tools/grit/',
    classifiers=[
        'Development Status :: 6 - Mature',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: MacOS',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Topic :: Utilities',
    ],
)
