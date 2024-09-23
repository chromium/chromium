# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import hashlib
import os
import re
import struct


class Util:
  """Helpers for generating C++."""

  @staticmethod
  def sanitize_name(name):
    return re.sub('[^0-9a-zA-Z_]', '_', name)

  @staticmethod
  def camel_to_snake(name):
    pat = '((?<=[a-z0-9])[A-Z]|(?!^)[A-Z](?=[a-z]))'
    return re.sub(pat, r'_\1', name).lower()

  @staticmethod
  def hash_name(name):
    # This must match the hash function in chromium's
    # //base/metrics/metric_hashes.cc. >Q means 8 bytes, big endian.
    name = name.encode('utf-8')
    md5 = hashlib.md5(name)
    return struct.unpack('>Q', md5.digest()[:8])[0]

  @staticmethod
  def event_name_hash(project_name, event_name, platform):
    """Make the name hash for an event.

    This gets uploaded in the StructuredEventProto.event_name_hash field. It is
    the sole means of recording which event from structured.xml a
    StructuredEventProto instance represents.

    To avoid naming collisions, it must contain three pieces of information:
     - the name of the event itself
     - the name of the event's project, to avoid collisions with events of the
       same name in other projects
     - an identifier that this comes from chromium, to avoid collisions with
       events and projects of the same name defined in cros's structured.xml

    This must use sanitized names for the project and event.
    """
    event_name = Util.sanitize_name(event_name)
    project_name = Util.sanitize_name(project_name)
    # TODO(crbug.com/40156926): Once the minimum python version is 3.6+, rewrite
    # this .format and others using f-strings.
    return Util.hash_name('{}::{}::{}'.format(platform, project_name,
                                              event_name))


class FileInfo:
  """Codegen-related info about a file."""

  def __init__(self, dirname, basename):
    self.dirname = dirname
    self.basename = basename
    self.rootname = os.path.splitext(self.basename)[0]
    self.filepath = os.path.join(dirname, basename)

    # This takes the last three components of the filepath for use in the
    # header guard, ie. METRICS_STRUCTURED_STRUCTURED_EVENTS_H_
    relative_path = os.sep.join(self.filepath.split(os.sep)[-3:])
    self.guard_path = Util.sanitize_name(relative_path).upper()
