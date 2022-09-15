# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
from typing import Dict, List, Tuple

import owners_data


def to_json_file(paths_with_data: List[
    Tuple[owners_data.RequestedPath, owners_data.PathData]],
                 output_path: str) -> None:
  '''Exports the data to an output json.'''

  with open(output_path, 'w') as f:
    for requested_path, path_data in paths_with_data:
      data_dict: Dict = _to_data_dict(requested_path, path_data)
      json.dump(data_dict, f)
      f.write('\n')


def _to_data_dict(requested_path: owners_data.RequestedPath,
                  path_data: owners_data.PathData) -> Dict:
  '''Transforms the RequestPath into a flat dictionary to be converted to json.
  '''

  def _joinppl(ppl, include_count=False):
    r = []
    for p in ppl:
      r.append(p[0] if not include_count else '{} ({})'.format(p[0], p[1]))
    return r

  owners = path_data.owner
  dir_metadata = path_data.dir_metadata
  git_data = path_data.git_data

  return {
      'path': requested_path.path,
      'feature': requested_path.feature,
      'owners_file': owners.owners_file,
      'owners_email': ', '.join(owners.owners),
      'team': dir_metadata.team if dir_metadata.team else '',
      'component': dir_metadata.component if dir_metadata.component else '',
      'os': dir_metadata.os if dir_metadata.os else '',
      'lines_of_code': str(git_data.lines_of_code),
      'number_of_files': str(git_data.number_of_files),
      'latest_cl_date': git_data.latest_cl_date,
      'cl_count': str(git_data.cls),
      'reverted_cl_count': str(git_data.reverted_cls),
      'relanded_cl_count': str(git_data.relanded_cls),
      'top_authors': ', '.join(_joinppl(git_data.get_top_authors(3))),
      'top_reviewers': ', '.join(_joinppl(git_data.get_top_reviewers(3))),
      'git_head': git_data.git_head,
      'git_head_time': git_data.git_head_time,
  }
