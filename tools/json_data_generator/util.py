# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path


def GetFileNameFromPath(file_path: str) -> str:
    return Path(file_path).name


def GetFileNameWithoutExtensionFromPath(file_path: str) -> str:
    return Path(file_path).stem


def GetDirNameFromPath(file_path: str) -> str:
    return str(Path(file_path).parent)


def JoinPath(dir_path: str, file_name: str) -> str:
    return str(Path(dir_path).joinpath(file_name))
