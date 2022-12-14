# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import dataclasses
import json
import os
import pathlib
import re
from typing import Callable, DefaultDict, Tuple


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
    # Use dict to ensure no duplicate pkgs.
    # In >=py3.7 dicts are ordered by insertion.
    self.packages = {}

    self.root = root
    self.link_prefix = link_prefix
    self.doc_namespace = doc_namespace
    self.read_file = read_file

    if not doc_name:
      doc_name = root_package_name
    self.doc_name = doc_name

  def add_package(self, name: str, license_file: str):
    """Add a package to the SPDX output."""
    self.packages[_Package(name, license_file)] = None

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

    return writer.write()


@dataclasses.dataclass(frozen=True)
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
  """Get relative path from the spdx root."""
  # remove rel path things in path
  abs_path = os.path.abspath(license_file_path)
  abs_root = os.path.abspath(root)
  if not abs_path.startswith(abs_root):
    raise ValueError(f'spdx root not valid. {abs_path} is not under {abs_root}')
  return abs_path[len(abs_root):]


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

    # Used to dedup license files based on file path.
    self.existing_license_files = {}  # 'file path': 'licenseId'
    # Used to make sure that there are no duplicate ids.
    self.existing_package_ids = collections.defaultdict(int)  # 'packageId': num
    self.existing_license_ids = collections.defaultdict(int)  # 'licenseId': num

    # Add the root package to make sure that its ID isn't taken.
    self.add_package(root_package)

  def write(self) -> str:
    """Returns a JSON string for the current state of the writer."""
    return json.dumps(self.content, indent=4)

  def _get_dedup_id(self, elem_id: str, id_dict: DefaultDict[str, int]) -> str:
    """Returns a unique id given a dictionary with existing ids.

    IDs are case sensitive, so this method ignores casing for uniqueness.

    Args:
      elem_id: the requested id to use for the element.
      id_dict: dictionary holding already used ids.

    Returns:
      When the elem_id is already unique, return elem_id.
      When the elem_id has been used, return elem_id + '-[next num]'.
    """
    suffix = id_dict[elem_id]
    id_dict[elem_id] += 1
    return f'{elem_id}-{suffix}' if suffix > 0 else elem_id

  def _get_package_id(self, pkg: _Package) -> str:
    """Makes sure that there are no pkg id duplicates."""
    return self._get_dedup_id(pkg.package_spdx_id, self.existing_package_ids)

  def _get_license_id(self, pkg: _Package) -> Tuple[str, bool]:
    """Handles license deduplication.

    If this pkg.file has already been seen, reuse that same id instead. If
    there are two packages with the same name but different license files,
    handle deduping the names.

    Args:
      pkg: The package to get a license id for.

    Returns:
      First return value is the id, second is whether the license needs to be
        added to the SPDX doc (False if it already exists in the doc).
    """
    existing = self.existing_license_files.get(pkg.file)
    if existing:
      return existing, False

    license_id = self._get_dedup_id(pkg.license_spdx_id,
                                    self.existing_license_ids)
    self.existing_license_files[pkg.file] = license_id
    return license_id, True

  def add_package(self, pkg: _Package):
    """Writes a package to the file (package metadata)."""
    pkg_id = self._get_package_id(pkg)
    license_id, need_to_add_license = self._get_license_id(pkg)

    self.content['packages'].append({
        'SPDXID': pkg_id,
        'name': pkg.name,
        'licenseConcluded': license_id,
    })

    if pkg.package_spdx_id != self.root_package_id:
      self.content['relationships'].append({
          'spdxElementId': self.root_package_id,
          'relationshipType': 'CONTAINS',
          'relatedSpdxElement': pkg_id,
      })

    if need_to_add_license:
      self._add_license_file(pkg, license_id)

  def _add_license_file(self, pkg: _Package, license_id: str):
    """Writes a license to the file (raw license text)."""
    spdx_path = _get_spdx_path(self.root, pkg.file)
    url = f'{self.link_prefix}{spdx_path.replace(os.sep, "/")}'
    self.content['hasExtractedLicensingInfos'].append({
        'name':
        f'{pkg.name}',
        'licenseId':
        license_id,
        'extractedText':
        self.read_file(pkg.file),
        'crossRefs': [{
            'url': url,
        }],
    })
