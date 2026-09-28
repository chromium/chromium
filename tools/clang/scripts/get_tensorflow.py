#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Credits to the The Fuchsia Authors for creating this file.

# /// script
# requires-python = '>=3.8,<3.9'
# dependencies = [
#   'absl-py==2.1.0',
#   'astunparse==1.6.3',
#   'cachetools==4.2.2',
#   'certifi==2021.5.30',
#   'charset-normalizer==2.0.4',
#   'flatbuffers==24.3.25',
#   'gast==0.4.0',
#   'google-auth-oauthlib==1.0.0',
#   'google-auth==2.16.2',
#   'google-pasta==0.2.0',
#   'grpcio==1.57.0',
#   'h5py==3.11.0',
#   'idna==3.2',
#   'importlib-metadata==8.0.0',
#   'jax==0.4.13',
#   'keras==2.12.0',
#   'libclang==18.1.1',
#   'markdown==3.3.4',
#   'markupsafe==2.1.5',
#   'ml-dtypes==0.2.0',
#   'numpy==1.22.1',
#   'oauthlib==3.2.2',
#   'opt-einsum==3.3.0',
#   'packaging==24.1',
#   'protobuf==4.25.1',
#   'pyasn1==0.4.8',
#   'pyasn1-modules==0.2.8',
#   'requests==2.31.0',
#   'requests-oauthlib==2.0.0',
#   'rsa==4.7.2',
#   'scipy==1.10.1',
#   'setuptools==70.3.0',
#   'six==1.16.0',
#   'tensorboard==2.12.3',
#   'tensorboard-data-server==0.7.2',
#   """tensorflow==2.12.0; sys_platform == 'linux' and \
#   platform_machine == 'x86_64'""",
#   'tensorflow-estimator==2.12.0',
#   'tensorflow-io-gcs-filesystem==0.34.0',
#   'termcolor==2.4.0',
#   'typing-extensions==4.0.1',
#   'urllib3==1.26.6',
#   'werkzeug==3.0.3',
#   'wheel==0.37.1',
#   'wrapt==1.14.1',
#   'zipp==3.7.0'
# ]
# ///

"""This script is used to fetch the tensorflow 2.7.0 pip package (via vpython)
for use by MLGO during LLVM compile. The vpython spec is hand-created, with the
help of pip. The transitive dependencies can be retrieved by using pipdeptree,
a pip package, by running `pipdeptree -p tensorflow`.
"""

import importlib
import os

spec = importlib.util.find_spec("tensorflow")
print(os.path.dirname(spec.origin))
