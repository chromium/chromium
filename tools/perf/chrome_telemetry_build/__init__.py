# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" This directory contains all the necessary files for clients in chromium/src
to use telemetry.

Those includes:
  - chromium_config module that defines a subclass of project_config, which
  specify binary dependencies & other references used by telemetry that are
  specific to chromium project.
  - binary_dependencies.json which specifies local paths to the binaries that
  are used by telemetry.
"""
