# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import dataclasses
from typing import List, Optional


@dataclasses.dataclass
class DirMetadata:
  '''A synthetic representation of a DIR_METADATA file.'''
  component: Optional[str] = None
  team: Optional[str] = None
  os: Optional[str] = None

  def copy(self):
    return dataclasses.replace(self)


@dataclasses.dataclass
class Owners:
  '''A synthetic representation of an OWNERS file.'''
  owners_file: str  # Path to OWNERS file
  file_inherited: Optional[str] = None  # Referenced OWNERS file
  owners: List[str] = dataclasses.field(default_factory=list)  # owners' emails


@dataclasses.dataclass
class GitData:
  '''Git data for a given hash/repo/folder.'''

  cls: int = 0
  reverted_cls: int = 0
  relanded_cls: int = 0
  lines_of_code: int = 0
  number_of_files: int = 0

  # key: ldap / value: # of cls
  authors: collections.Counter = dataclasses.field(
      default_factory=collections.Counter)
  reviewers: collections.Counter = dataclasses.field(
      default_factory=collections.Counter)

  latest_cl_date: Optional[int] = None
  git_head: Optional[str] = None
  git_head_time: Optional[str] = None

  def get_top_authors(self, n):
    return self.authors.most_common(n)

  def get_top_reviewers(self, n):
    return self.reviewers.most_common(n)


@dataclasses.dataclass(frozen=True)
class RequestedPath:
  '''Path to be searched for.'''
  path: str
  feature: str


@dataclasses.dataclass(frozen=True)
class PathData:
  '''Path to be searched for.'''
  owner: Owners
  git_data: GitData
  dir_metadata: DirMetadata
