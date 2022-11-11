# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import json
import os
import pathlib
import re
from typing import Callable


class SpdxWriter:
  """Accepts package metadata and outputs licensing info in SPDX format."""

  def __init__(self,
               root: str,
               root_package_name: str,
               root_license: str,
               link_prefix: str,
               doc_name: str = None,
               doc_namespace: str = None,
               read_file=lambda x: pathlib.Path(x).read_text(encoding='utf-8')):
    self.root_package = _Package(root_package_name, root_license)
    self.packages = [self.root_package]

    self.root = root
    self.link_prefix = link_prefix
    self.doc_namespace = doc_namespace
    self.read_file = read_file

    if not doc_name:
      doc_name = root_package_name
    self.doc_name = doc_name

  def add_package(self, name: str, license_file: str):
    """Add a package to the SPDX output."""
    self.packages.append(_Package(name, license_file))

  def write_to_file(self, file_path: str):
    """Writes the content to a file."""
    with open(file_path, 'w', encoding='utf-8') as f:
      f.write(self.write())

  def write(self) -> str:
    """Writes out SPDX in JSON format."""
    writer = _SPDXJSONWriter(self.root, self.root_package, self.link_prefix,
                             self.doc_name, self.doc_namespace, self.read_file)

    for pkg in self.packages:
      writer.add_package(pkg)

    for pkg in self.packages:
      writer.add_license_file(pkg)

    return writer.write()


@dataclasses.dataclass
class _Package:
  """Stores needed data for a package to output SPDX."""
  name: str
  file: str

  @property
  def package_spdx_id(self) -> str:
    return self._escape_id(f'SPDXRef-Package-{self.name}')

  def _escape_id(self, spdx_id: str) -> str:
    return re.sub(r'[^a-zA-Z0-9-\.]', '-', spdx_id)

  @property
  def license_spdx_id(self) -> str:
    return self._escape_id(f'LicenseRef-{self.name}')


def _get_spdx_path(root: str, license_file_path: str) -> str:
  """Get path from the spdx root."""
  # remove rel path things in path
  abs_path = os.path.abspath(license_file_path)
  if not abs_path.startswith(root):
    raise ValueError(f'spdx root not valid. {abs_path} is not under root')
  return abs_path[len(root):]


class _SPDXJSONWriter():
  """Writes SPDX data in JSON format.

  Produce SPDX JSON output adherring to this schema:
    https://github.com/spdx/spdx-spec/blob/development/v2.2.2/schemas/spdx-schema.json
    See example:
    https://github.com/spdx/spdx-spec/blob/development/v2.2.2/examples/SPDXJSONExample-v2.2.spdx.json
  """

  def __init__(self, root: str, root_package: _Package, link_prefix: str,
               doc_name: str, doc_namespace: str,
               read_file: Callable[[str], str]):
    self.root = root
    self.root_package_id = root_package.package_spdx_id
    self.link_prefix = link_prefix

    self.read_file = read_file

    self.content = {
        # Actually 2.2.2, but only SPDX-N.M is needed.
        'spdxVersion': 'SPDX-2.2',
        'SPDXID': 'SPDXRef-DOCUMENT',
        'name': doc_name,
        'documentNamespace': doc_namespace,
        'creationInfo': {
            'creators': [f'Tool: {os.path.basename(__file__)}'],
        },
        'dataLicense': 'CC0-1.0',
        'documentDescribes': [self.root_package_id],
        'packages': [],
        'hasExtractedLicensingInfos': [],
        'relationships': [],
    }

  def write(self) -> str:
    """Returns a JSON string for the current state of the writer."""
    return json.dumps(self.content, indent=4)

  def add_package(self, pkg: _Package):
    """Writes a package to the file (package metadata)."""
    self.content['packages'].append({
        'SPDXID': pkg.package_spdx_id,
        'name': pkg.name,
        'licenseConcluded': pkg.license_spdx_id,
    })

    if pkg.package_spdx_id != self.root_package_id:
      self.content['relationships'].append({
          'spdxElementId':
          self.root_package_id,
          'relationshipType':
          'CONTAINS',
          'relatedSpdxElement':
          pkg.package_spdx_id,
      })

  def add_license_file(self, pkg: _Package):
    """Writes a license to the file (raw license text)."""
    self.content['hasExtractedLicensingInfos'].append({
        'name':
        f'{pkg.name} License',
        'licenseId':
        pkg.license_spdx_id,
        'extractedText':
        self.read_file(pkg.file),
        'crossRefs': [{
            'url':
            f'{self.link_prefix}{_get_spdx_path(self.root, pkg.file)}',
        }],
    })
