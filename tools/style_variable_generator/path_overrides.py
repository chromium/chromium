# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


# Returns the FileSystemLoader root directory. Overridden in google3.
def GetFileSystemLoaderRootDirectory():
    return os.path.dirname(os.path.realpath(__file__))


# Returns the path to the template relative to FileSystemLoader root.
# Overridden in google3.
def GetPathToTemplate(path_to_template):
    return path_to_template
