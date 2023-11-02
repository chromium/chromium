#!/usr/bin/env python3

# Copyright 2020 The Crashpad Authors
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
import functools
import hashlib
import io
import json
import os
import re
import shutil
import string
import subprocess
import sys
import tempfile
import xml.etree.ElementTree


LLDBINIT_PATH = '$(PROJECT_DIR)/.lldbinit'

PYTHON_RE = re.compile('[ /]python[23]?$')

XCTEST_PRODUCT_TYPES = frozenset((
    'com.apple.product-type.bundle.unit-test',
    'com.apple.product-type.bundle.ui-testing',
))

SCHEME_PRODUCT_TYPES = frozenset((
    'com.apple.product-type.app-extension',
    'com.apple.product-type.application',
    'com.apple.product-type.framework'
))


class Template(string.Template):

  """A subclass of string.Template that changes delimiter."""

  delimiter = '@'


@functools.lru_cache
def LoadSchemeTemplate(root, name):
  """Return a string.Template object for scheme file loaded relative to root."""
  path = os.path.join(root, 'build', 'ios', name + '.template')
  with open(path) as file:
    return Template(file.read())


def CreateIdentifier(str_id):
  """Return a 24 characters string that can be used as an identifier."""
  return hashlib.sha1(str_id.encode("utf-8")).hexdigest()[:24].upper()


def GenerateSchemeForTarget(root, project, old_project, name, path, is_test):
  """Generates the .xcsheme file for target named |name|.

  The file is generated in the new project schemes directory from a template.
  If there is an existing previous project, then the old scheme file is copied
  and the lldbinit setting is set. If lldbinit setting is already correct, the
  file is not modified, just copied.
  """
  project_name = os.path.basename(project)
  relative_path = os.path.join('xcshareddata', 'xcschemes', name + '.xcscheme')
  identifier = CreateIdentifier('%s %s' % (name, path))

  scheme_path = os.path.join(project, relative_path)
  if not os.path.isdir(os.path.dirname(scheme_path)):
    os.makedirs(os.path.dirname(scheme_path))

  substitutions = {
    'LLDBINIT_PATH': LLDBINIT_PATH,
    'BLUEPRINT_IDENTIFIER': identifier,
    'BUILDABLE_NAME': path,
    'BLUEPRINT_NAME': name,
    'PROJECT_NAME': project_name
  }

  if is_test:
    template = LoadSchemeTemplate(root, 'xcodescheme-testable')
    substitutions['PATH'] = os.environ['PATH']

  else:
    template = LoadSchemeTemplate(root, 'xcodescheme')

  old_scheme_path = os.path.join(old_project, relative_path)
  if os.path.exists(old_scheme_path):

    tree = xml.etree.ElementTree.parse(old_scheme_path)
    tree_root = tree.getroot()

    for reference in tree_root.findall('.//BuildableReference'):
      for (attr, value) in (
          ('BuildableName', path),
          ('BlueprintName', name),
          ('BlueprintIdentifier', identifier)):
        if reference.get(attr) != value:
          reference.set(attr, value)

    for child in tree_root:
      if child.tag not in ('TestAction', 'LaunchAction'):
        continue

      if child.get('customLLDBInitFile') != LLDBINIT_PATH:
        child.set('customLLDBInitFile', LLDBINIT_PATH)

    if is_test:

      template_tree = xml.etree.ElementTree.parse(
          io.StringIO(template.substitute(**substitutions)))

      template_tree_root = template_tree.getroot()
      for child in tree_root:
        if child.tag != 'BuildAction':
          continue

        for subchild in list(child):
          child.remove(subchild)

        for post_action in template_tree_root.findall('.//PostActions'):
          child.append(post_action)

    tree.write(scheme_path, xml_declaration=True, encoding='UTF-8')

  else:

    with open(scheme_path, 'w') as scheme_file:
      scheme_file.write(template.substitute(**substitutions))


class XcodeProject(object):

  def __init__(self, objects, counter = 0):
    self.objects = objects
    self.counter = 0

  def AddObject(self, parent_name, obj):
    while True:
      self.counter += 1
      str_id = "%s %s %d" % (parent_name, obj['isa'], self.counter)
      new_id = CreateIdentifier(str_id)

      # Make sure ID is unique. It's possible there could be an id conflict
      # since this is run after GN runs.
      if new_id not in self.objects:
        self.objects[new_id] = obj
        return new_id

  def IterObjectsByIsa(self, isa):
    """Iterates overs objects of the |isa| type."""
    for key, obj in self.objects.items():
      if obj['isa'] == isa:
        yield (key, obj)

  def IterNativeTargetByProductType(self, product_types):
    """Iterates over PBXNativeTarget objects of any |product_types| types."""
    for key, obj in self.IterObjectsByIsa('PBXNativeTarget'):
      if obj['productType'] in product_types:
        yield (key, obj)

  def UpdateBuildScripts(self):
    """Update build scripts to respect configuration and platforms."""
    for key, obj in self.IterObjectsByIsa('PBXShellScriptBuildPhase'):

      shell_path = obj['shellPath']
      shell_code = obj['shellScript']
      if shell_path.endswith('/sh'):
        shell_code = shell_code.replace(
            'ninja -C .',
            'ninja -C "../${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}"')
      elif PYTHON_RE.search(shell_path):
        shell_code = shell_code.replace(
            '''ninja_params = [ '-C', '.' ]''',
            '''ninja_params = [ '-C', '../' + os.environ['CONFIGURATION']'''
            ''' + os.environ['EFFECTIVE_PLATFORM_NAME'] ]''')

      # Replace the build script in the object.
      obj['shellScript'] = shell_code


  def UpdateBuildConfigurations(self, configurations):
    """Add new configurations, using the first one as default."""

    # Create a list with all the objects of interest. This is needed
    # because objects will be added to/removed from the project upon
    # iterating this list and python dictionaries cannot be mutated
    # during iteration.

    for key, obj in list(self.IterObjectsByIsa('XCConfigurationList')):
      # Use the first build configuration as template for creating all the
      # new build configurations.
      build_config_template = self.objects[obj['buildConfigurations'][0]]
      build_config_template['buildSettings']['CONFIGURATION_BUILD_DIR'] = \
          '$(PROJECT_DIR)/../$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)'

      # Remove the existing build configurations from the project before
      # creating the new ones.
      for build_config_id in obj['buildConfigurations']:
        del self.objects[build_config_id]
      obj['buildConfigurations'] = []

      for configuration in configurations:
        build_config = copy.copy(build_config_template)
        build_config['name'] = configuration
        build_config_id = self.AddObject('products', build_config)
        obj['buildConfigurations'].append(build_config_id)

  def GetHostMappingForXCTests(self):
    """Returns a dict from targets to the list of their xctests modules."""
    mapping = collections.defaultdict(list)
    for key, obj in self.IterNativeTargetByProductType(XCTEST_PRODUCT_TYPES):
      build_config_lists_id = obj['buildConfigurationList']
      build_configs = self.objects[build_config_lists_id]['buildConfigurations']

      # Use the first build configuration to get the name of the host target.
      # This is arbitrary, but since the build configuration are all identical
      # after UpdateBuildConfiguration, except for their 'name', it is fine.
      build_config = self.objects[build_configs[0]]
      if obj['productType'] == 'com.apple.product-type.bundle.unit-test':
        # The test_host value will look like this:
        # `$(BUILD_PRODUCTS_DIR)/host_app_name.app/host_app_name`
        #
        # Extract the `host_app_name.app` part as key for the output.
        test_host_path = build_config['buildSettings']['TEST_HOST']
        test_host_name = os.path.basename(os.path.dirname(test_host_path))
      else:
        test_host_name = build_config['buildSettings']['TEST_TARGET_NAME']

      test_name = obj['name']
      test_path = self.objects[obj['productReference']]['path']

      mapping[test_host_name].append((key, test_name, test_path))

    return dict(mapping)


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


def UpdateXcodeProject(project_dir, old_project_dir, configurations, root_dir):
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

  project.UpdateBuildScripts()
  project.UpdateBuildConfigurations(configurations)

  mapping = project.GetHostMappingForXCTests()

  # Generate schemes for application, extensions and framework targets
  for key, obj in project.IterNativeTargetByProductType(SCHEME_PRODUCT_TYPES):
    product = project.objects[obj['productReference']]
    product_path = product['path']

    # Do not generate scheme for the XCTests and XXCUITests target app.
    # Instead, a scheme will be generated for each test modules.
    tests = mapping.get(product_path, []) + mapping.get(obj['name'], [])
    if not tests:
      GenerateSchemeForTarget(
          root_dir, project_dir, old_project_dir,
          obj['name'], product_path, False)

    else:
      for (_, test_name, test_path) in tests:
        GenerateSchemeForTarget(
          root_dir, project_dir, old_project_dir,
          test_name, test_path, True)

  root_object = project.objects[json_data['rootObject']]
  main_group = project.objects[root_object['mainGroup']]

  sources = None
  for child_key in main_group['children']:
    child = project.objects[child_key]
    if child.get('name') == 'Source' or child.get('name') == 'Sources':
      sources = child
      break

  if sources is None:
    sources = main_group

  AddMarkdownToProject(project, root_dir, sources, sources is main_group)
  SortFileReferencesByName(project, sources, root_object.get('productRefGroup'))

  objects = collections.OrderedDict(sorted(project.objects.items()))
  # WriteXcodeProject(project_dir, json.dumps(json_data))


def CreateGroup(project, parent_group, group_name, use_relative_paths):
  group_object = {
    'children': [],
    'isa': 'PBXGroup',
    'sourceTree': '<group>',
  }
  if use_relative_paths:
    group_object['path'] = group_name
  else:
    group_object['name'] = group_name
  parent_group_name = parent_group.get('name', '')
  group_object_key = project.AddObject(parent_group_name, group_object)
  parent_group['children'].append(group_object_key)
  return group_object


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

  def __init__(self, obj, last):
    self.isa = obj['isa']
    if 'name' in obj:
      self.name = obj['name']
    else:
      self.name = obj['path']
    self.last = last

  def __lt__(self, other):
    if self.last != other.last:
      return other.last
    if self.isa != other.isa:
      return self.isa > other.isa
    return self.name < other.name

  def __gt__(self, other):
    if self.last != other.last:
      return self.last
    if self.isa != other.isa:
      return self.isa < other.isa
    return self.name > other.name

  def __eq__(self, other):
    return self.isa == other.isa and self.name == other.name


def SortFileReferencesByName(project, group_object, products_group_ref):
  SortFileReferencesByNameWithSortKey(
      project, group_object,
      lambda ref: ObjectKey(project.objects[ref], ref == products_group_ref))


def SortFileReferencesByNameWithSortKey(project, group_object, sort_key):
  group_object['children'].sort(key=sort_key)
  for key in group_object['children']:
    child = project.objects[key]
    if child['isa'] == 'PBXGroup':
      SortFileReferencesByNameWithSortKey(project, child, sort_key)


def AddMarkdownToProject(project, root_dir, group_object, use_relative_paths):
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
      "sourceTree": "<group>"
    }
    if use_relative_paths:
      new_markdown_entry['path'] = os.path.basename(path)
    else:
      new_markdown_entry['name'] = os.path.basename(path)
      new_markdown_entry['path'] = path
    folder = GetFolderForPath(
        project, group_object, os.path.dirname(path),
        use_relative_paths)
    folder_name = folder.get('name', None)
    if folder_name is None:
      folder_name = folder.get('path', 'sources')
    new_markdown_entry_id = project.AddObject(folder_name, new_markdown_entry)
    folder['children'].append(new_markdown_entry_id)


def GetFolderForPath(project, group_object, path, use_relative_paths):
  objects = project.objects
  if not path:
    return group_object
  for folder in path.split('/'):
    children = group_object['children']
    new_root = None
    for child_key in children:
      child = objects[child_key]
      if child['isa'] == 'PBXGroup':
        child_name = child.get('name', None)
        if child_name is None:
          child_name = child.get('path')
        if child_name == folder:
          new_root = child
          break
    if not new_root:
      # If the folder isn't found we could just cram it into the leaf existing
      # folder, but that leads to folders with tons of README.md inside.
      new_root = CreateGroup(project, group_object, folder, use_relative_paths)
    group_object = new_root
  return group_object


def ConvertGnXcodeProject(root_dir, proj_name, input_dir, output_dir, configs):
  '''Tweak the Xcode project generated by gn to support multiple configurations.

  The Xcode projects generated by "gn gen --ide" only supports a single
  platform and configuration (as the platform and configuration are set
  per output directory). This method takes as input such projects and
  add support for multiple configurations and platforms (to allow devs
  to select them in Xcode).

  Args:
    root_dir: directory that is the root of the project
    proj_name: name of the Xcode project "file" (usually `all.xcodeproj`)
    input_dir: directory containing the XCode projects created by "gn gen --ide"
    output_dir: directory where the tweaked Xcode projects will be saved
    configs: list of string corresponding to the configurations that need to be
        supported by the tweaked Xcode projects, must contains at least one
        value.
  '''

  UpdateXcodeProject(
      os.path.join(input_dir, proj_name),
      os.path.join(output_dir, proj_name),
      configs, root_dir)

  CopyTreeIfChanged(os.path.join(input_dir, proj_name),
                    os.path.join(output_dir, proj_name))


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
  parser.add_argument(
      '--project-name', default='all.xcodeproj', dest='proj_name',
      help='name of the Xcode project (default: %(default)s)')
  args = parser.parse_args(args)

  if not os.path.isdir(args.input):
    sys.stderr.write('Input directory does not exists.\n')
    return 1

  if args.proj_name not in os.listdir(args.input):
    sys.stderr.write(
        'Input directory does not contain the Xcode project.\n')
    return 1

  if not args.configurations:
    sys.stderr.write('At least one configuration required, see --add-config.\n')
    return 1

  ConvertGnXcodeProject(
      args.root,
      args.proj_name,
      args.input,
      args.output,
      args.configurations)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
