#!/usr/bin/env python

# Copyright 2020 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Convert GN Xcode projects to platform and configuration independent targets.

GN generates Xcode projects that build one configuration only. However, typical
iOS development involves using the Xcode IDE to toggle the platform and
configuration. This script replaces the 'gn' configuration with 'Debug',
'Release' and 'Profile', and changes the ninja invocation to honor these
configurations.
"""

import argparse
import collections
import copy
import filecmp
import json
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile


class XcodeProject(object):

  def __init__(self, objects, counter = 0):
    self.objects = objects
    self.counter = 0

  def AddObject(self, parent_name, obj):
    while True:
      self.counter += 1
      str_id = "%s %s %d" % (parent_name, obj['isa'], self.counter)
      new_id = hashlib.sha1(str_id.encode("utf-8")).hexdigest()[:24].upper()

      # Make sure ID is unique. It's possible there could be an id conflict
      # since this is run after GN runs.
      if new_id not in self.objects:
        self.objects[new_id] = obj
        return new_id


def check_output(command):
  """Wrapper around subprocess.check_output that decode output as utf-8."""
  return subprocess.check_output(command).decode('utf-8')


def CopyFileIfChanged(source_path, target_path):
  """Copy |source_path| to |target_path| if different."""
  target_dir = os.path.dirname(target_path)
  if not os.path.isdir(target_dir):
    os.makedirs(target_dir)
  if not os.path.exists(target_path) or \
      not filecmp.cmp(source_path, target_path):
    shutil.copyfile(source_path, target_path)


def CopyTreeIfChanged(source, target):
  """Copy |source| to |target| recursively; files are copied iff changed."""
  if os.path.isfile(source):
    return CopyFileIfChanged(source, target)
  if not os.path.isdir(target):
    os.makedirs(target)
  for name in os.listdir(source):
    CopyTreeIfChanged(
        os.path.join(source, name),
        os.path.join(target, name))


def LoadXcodeProjectAsJSON(project_dir):
  """Return Xcode project at |path| as a JSON string."""
  return check_output([
      'plutil', '-convert', 'json', '-o', '-',
      os.path.join(project_dir, 'project.pbxproj')])


def WriteXcodeProject(output_path, json_string):
  """Save Xcode project to |output_path| as XML."""
  with tempfile.NamedTemporaryFile() as temp_file:
    temp_file.write(json_string.encode("utf-8"))
    temp_file.flush()
    subprocess.check_call(['plutil', '-convert', 'xml1', temp_file.name])
    CopyFileIfChanged(
        temp_file.name,
        os.path.join(output_path, 'project.pbxproj'))


def UpdateXcodeProject(project_dir, configurations, root_dir):
  """Update inplace Xcode project to support multiple configurations.

  Args:
    project_dir: path to the input Xcode project
    configurations: list of string corresponding to the configurations that
      need to be supported by the tweaked Xcode projects, must contains at
      least one value.
    root_dir: path to the root directory used to find markdown files
  """
  json_data = json.loads(LoadXcodeProjectAsJSON(project_dir))
  project = XcodeProject(json_data['objects'])

  objects_to_remove = []
  for value in list(project.objects.values()):
    isa = value['isa']

    # Teach build shell script to look for the configuration and platform.
    if isa == 'PBXShellScriptBuildPhase':
      shell_path = value['shellPath']
      if shell_path.endswith('/sh'):
        value['shellScript'] = value['shellScript'].replace(
            'ninja -C .',
            'ninja -C "../${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}"')
      elif re.search('[ /]python[23]?$', shell_path):
        value['shellScript'] = value['shellScript'].replace(
            'ninja_params = [ \'-C\', \'.\' ]',
            'ninja_params = [ \'-C\', \'../\' + os.environ[\'CONFIGURATION\']'
            ' + os.environ[\'EFFECTIVE_PLATFORM_NAME\'] ]')

    # Add new configuration, using the first one as default.
    if isa == 'XCConfigurationList':
      value['defaultConfigurationName'] = configurations[0]
      objects_to_remove.extend(value['buildConfigurations'])

      build_config_template = project.objects[value['buildConfigurations'][0]]
      build_config_template['buildSettings']['CONFIGURATION_BUILD_DIR'] = \
          '$(PROJECT_DIR)/../$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)'

      value['buildConfigurations'] = []
      for configuration in configurations:
        new_build_config = copy.copy(build_config_template)
        new_build_config['name'] = configuration
        value['buildConfigurations'].append(
            project.AddObject('products', new_build_config))

  for object_id in objects_to_remove:
    del project.objects[object_id]

  source = GetOrCreateRootGroup(project, json_data['rootObject'], 'Source')
  AddMarkdownToProject(project, root_dir, source)
  SortFileReferencesByName(project, source)

  objects = collections.OrderedDict(sorted(project.objects.items()))
  WriteXcodeProject(project_dir, json.dumps(json_data))


def CreateGroup(project, parent_group, group_name, path=None):
  group_object = {
    'children': [],
    'isa': 'PBXGroup',
    'name': group_name,
    'sourceTree': '<group>',
  }
  if path is not None:
    group_object['path'] = path
  parent_group_name = parent_group.get('name', '')
  group_object_key = project.AddObject(parent_group_name, group_object)
  parent_group['children'].append(group_object_key)
  return group_object


def GetOrCreateRootGroup(project, root_object, group_name):
  main_group = project.objects[project.objects[root_object]['mainGroup']]
  for child_key in main_group['children']:
    child = project.objects[child_key]
    if child['name'] == group_name:
      return child
  return CreateGroup(project, main_group, group_name, path='../..')


class ObjectKey(object):

  """Wrapper around PBXFileReference and PBXGroup for sorting.

  A PBXGroup represents a "directory" containing a list of files in an
  Xcode project; it can contain references to a list of directories or
  files.

  A PBXFileReference represents a "file".

  The type is stored in the object "isa" property as a string. Since we
  want to sort all directories before all files, the < and > operators
  are defined so that if "isa" is different, they are sorted in the
  reverse of alphabetic ordering, otherwise the name (or path) property
  is checked and compared in alphabetic order.
  """

  def __init__(self, obj):
    self.isa = obj['isa']
    if 'name' in obj:
      self.name = obj['name']
    else:
      self.name = obj['path']

  def __lt__(self, other):
    if self.isa != other.isa:
      return self.isa > other.isa
    return self.name < other.name

  def __gt__(self, other):
    if self.isa != other.isa:
      return self.isa < other.isa
    return self.name > other.name

  def __eq__(self, other):
    return self.isa == other.isa and self.name == other.name


def SortFileReferencesByName(project, group_object):
  SortFileReferencesByNameWithSortKey(
      project, group_object, lambda ref: ObjectKey(project.objects[ref]))


def SortFileReferencesByNameWithSortKey(project, group_object, sort_key):
  group_object['children'].sort(key=sort_key)
  for key in group_object['children']:
    child = project.objects[key]
    if child['isa'] == 'PBXGroup':
      SortFileReferencesByNameWithSortKey(project, child, sort_key)


def AddMarkdownToProject(project, root_dir, group_object):
  list_files_cmd = ['git', '-C', root_dir, 'ls-files', '*.md']
  paths = check_output(list_files_cmd).splitlines()
  ios_internal_dir = os.path.join(root_dir, 'ios_internal')
  if os.path.exists(ios_internal_dir):
    list_files_cmd = ['git', '-C', ios_internal_dir, 'ls-files', '*.md']
    ios_paths = check_output(list_files_cmd).splitlines()
    paths.extend([os.path.join("ios_internal", path) for path in ios_paths])
  for path in paths:
    new_markdown_entry = {
      "fileEncoding": "4",
      "isa": "PBXFileReference",
      "lastKnownFileType": "net.daringfireball.markdown",
      "name": os.path.basename(path),
      "path": path,
      "sourceTree": "<group>"
    }
    new_markdown_entry_id = project.AddObject('sources', new_markdown_entry)
    folder = GetFolderForPath(project, group_object, os.path.dirname(path))
    folder['children'].append(new_markdown_entry_id)


def GetFolderForPath(project, group_object, path):
  objects = project.objects
  if not path:
    return group_object
  for folder in path.split('/'):
    children = group_object['children']
    new_root = None
    for child in children:
      if objects[child]['isa'] == 'PBXGroup' and \
         objects[child]['name'] == folder:
        new_root = objects[child]
        break
    if not new_root:
      # If the folder isn't found we could just cram it into the leaf existing
      # folder, but that leads to folders with tons of README.md inside.
      new_root = CreateGroup(project, group_object, folder)
    group_object = new_root
  return group_object


def ConvertGnXcodeProject(root_dir, input_dir, output_dir, configurations):
  '''Tweak the Xcode project generated by gn to support multiple configurations.

  The Xcode projects generated by "gn gen --ide" only supports a single
  platform and configuration (as the platform and configuration are set
  per output directory). This method takes as input such projects and
  add support for multiple configurations and platforms (to allow devs
  to select them in Xcode).

  Args:
    input_dir: directory containing the XCode projects created by "gn gen --ide"
    output_dir: directory where the tweaked Xcode projects will be saved
    configurations: list of string corresponding to the configurations that
      need to be supported by the tweaked Xcode projects, must contains at
      least one value.
  '''

  # Update the project (supports legacy name "products.xcodeproj" or the new
  # project name "all.xcodeproj").
  for project_name in ('all.xcodeproj', 'products.xcodeproj'):
    if os.path.exists(os.path.join(input_dir, project_name)):
      UpdateXcodeProject(
          os.path.join(input_dir, project_name),
          configurations, root_dir)

      CopyTreeIfChanged(os.path.join(input_dir, project_name),
                        os.path.join(output_dir, project_name))

    else:
      shutil.rmtree(os.path.join(output_dir, project_name), ignore_errors=True)

  # Copy all.xcworkspace if it exists (will be removed in a future gn version).
  workspace_name = 'all.xcworkspace'
  if os.path.exists(os.path.join(input_dir, workspace_name)):
    CopyTreeIfChanged(os.path.join(input_dir, workspace_name),
                      os.path.join(output_dir, workspace_name))
  else:
    shutil.rmtree(os.path.join(output_dir, workspace_name), ignore_errors=True)


def Main(args):
  parser = argparse.ArgumentParser(
      description='Convert GN Xcode projects for iOS.')
  parser.add_argument(
      'input',
      help='directory containing [product|all] Xcode projects.')
  parser.add_argument(
      'output',
      help='directory where to generate the iOS configuration.')
  parser.add_argument(
      '--add-config', dest='configurations', default=[], action='append',
      help='configuration to add to the Xcode project')
  parser.add_argument(
      '--root', type=os.path.abspath, required=True,
      help='root directory of the project')
  args = parser.parse_args(args)

  if not os.path.isdir(args.input):
    sys.stderr.write('Input directory does not exists.\n')
    return 1

  # Depending on the version of "gn", there should be either one project file
  # named "all.xcodeproj" or a project file named "products.xcodeproj" and a
  # workspace named "all.xcworkspace".
  required_files_sets = [
      set(("all.xcodeproj",)),
      set(("products.xcodeproj", "all.xcworkspace")),
  ]

  for required_files in required_files_sets:
    if required_files.issubset(os.listdir(args.input)):
      break
  else:
    sys.stderr.write(
        'Input directory does not contain all necessary Xcode projects.\n')
    return 1

  if not args.configurations:
    sys.stderr.write('At least one configuration required, see --add-config.\n')
    return 1

  ConvertGnXcodeProject(args.root, args.input, args.output, args.configurations)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
