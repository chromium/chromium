#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Credits to the The Fuchsia Authors for creating this file.
"""This script is used to fetch the tensorflow 2.7.0 pip package (via vpython)
for use by MLGO during LLVM compile. The vpython spec is hand-created, with the
help of pip. The transitive dependencies can be retrieved by using pipdeptree,
a pip package, by running `pipdeptree -p tensorflow`.
"""
# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/absl-py-py3"
#   version: "version:2.1.0"
# >
# wheel: <
#   name: "infra/python/wheels/astunparse-py2_py3"
#   version: "version:1.6.3"
# >
# wheel: <
#   name: "infra/python/wheels/cachetools-py3"
#   version: "version:4.2.2"
# >
# wheel: <
#   name: "infra/python/wheels/certifi-py2_py3"
#   version: "version:2021.5.30"
# >
# wheel: <
#   name: "infra/python/wheels/charset_normalizer-py3"
#   version: "version:2.0.4"
# >
# wheel: <
#   name: "infra/python/wheels/flatbuffers-py3"
#   version: "version:24.3.25"
# >
# wheel: <
#   name: "infra/python/wheels/gast-py3"
#   version: "version:0.4.0"
# >
# wheel: <
#   name: "infra/python/wheels/google-auth-oauthlib-py3"
#   version: "version:1.0.0"
# >
# wheel: <
#   name: "infra/python/wheels/google-auth-py3"
#   version: "version:2.16.2"
# >
# wheel: <
#   name: "infra/python/wheels/google-pasta-py3"
#   version: "version:0.2.0"
# >
# wheel: <
#   name: "infra/python/wheels/grpcio/${vpython_platform}"
#   version: "version:1.57.0"
# >
# wheel: <
#   name: "infra/python/wheels/h5py/${vpython_platform}"
#   version: "version:3.11.0"
# >
# wheel: <
#   name: "infra/python/wheels/idna-py3"
#   version: "version:3.2"
# >
# wheel: <
#   name: "infra/python/wheels/importlib-metadata-py3"
#   version: "version:8.0.0"
# >
# wheel: <
#   name: "infra/python/wheels/jax-py3"
#   version: "version:0.4.13"
# >
# wheel: <
#   name: "infra/python/wheels/keras-py3"
#   version: "version:2.12.0"
# >
# wheel: <
#   name: "infra/python/wheels/libclang/${vpython_platform}"
#   version: "version:18.1.1"
# >
# wheel: <
#   name: "infra/python/wheels/markdown-py3"
#   version: "version:3.3.4"
# >
# wheel: <
#   name: "infra/python/wheels/markupsafe/${vpython_platform}"
#   version: "version:2.1.5"
# >
# wheel: <
#   name: "infra/python/wheels/ml_dtypes/${vpython_platform}"
#   version: "version:0.2.0"
# >
# wheel: <
#   name: "infra/python/wheels/numpy/${vpython_platform}"
#   version: "version:1.22.1"
# >
# wheel: <
#   name: "infra/python/wheels/oauthlib-py2_py3"
#   version: "version:3.2.2"
# >
# wheel: <
#   name: "infra/python/wheels/opt-einsum-py3"
#   version: "version:3.3.0"
# >
# wheel: <
#   name: "infra/python/wheels/packaging-py3"
#   version: "version:24.1"
# >
# wheel: <
#   name: "infra/python/wheels/protobuf-py3"
#   version: "version:4.25.1"
# >
# wheel: <
#   name: "infra/python/wheels/pyasn1-py2_py3"
#   version: "version:0.4.8"
# >
# wheel: <
#   name: "infra/python/wheels/pyasn1_modules-py2_py3"
#   version: "version:0.2.8"
# >
# wheel: <
#   name: "infra/python/wheels/requests-py3"
#   version: "version:2.31.0"
# >
# wheel: <
#   name: "infra/python/wheels/requests-oauthlib-py2_py3"
#   version: "version:2.0.0"
# >
# wheel: <
#   name: "infra/python/wheels/rsa-py3"
#   version: "version:4.7.2"
# >
# wheel: <
#   name: "infra/python/wheels/scipy/${vpython_platform}"
#   version: "version:1.10.1"
# >
# wheel: <
#   name: "infra/python/wheels/setuptools-py3"
#   version: "version:70.3.0"
# >
# wheel: <
#   name: "infra/python/wheels/six-py2_py3"
#   version: "version:1.16.0"
# >
# wheel: <
#   name: "infra/python/wheels/tensorboard-py3"
#   version: "version:2.12.3"
# >
# wheel: <
#   name: "infra/python/wheels/tensorboard-data-server-py3"
#   version: "version:0.7.2"
# >
# wheel: <
#   name: "infra/python/wheels/tensorflow/${vpython_platform}"
#   version: "version:2.12.0"
# >
# wheel: <
#   name: "infra/python/wheels/tensorflow-estimator-py3"
#   version: "version:2.12.0"
# >
# wheel: <
#   name: "infra/python/wheels/tensorflow-io-gcs-filesystem/${vpython_platform}"
#   version: "version:0.34.0"
# >
# wheel: <
#   name: "infra/python/wheels/termcolor-py2_py3"
#   version: "version:2.4.0"
# >
# wheel: <
#   name: "infra/python/wheels/typing-extensions-py3"
#   version: "version:4.0.1"
# >
# wheel: <
#   name: "infra/python/wheels/urllib3-py2_py3"
#   version: "version:1.26.6"
# >
# wheel: <
#   name: "infra/python/wheels/werkzeug-py3"
#   version: "version:3.0.3"
# >
# wheel: <
#   name: "infra/python/wheels/wheel-py2_py3"
#   version: "version:0.37.1"
# >
# wheel: <
#   name: "infra/python/wheels/wrapt/${vpython_platform}"
#   version: "version:1.14.1"
# >
# wheel: <
#   name: "infra/python/wheels/zipp-py3"
#   version: "version:3.7.0"
# >

# [VPYTHON:END]
import importlib
import os

spec = importlib.util.find_spec("tensorflow")
print(os.path.dirname(spec.origin))
