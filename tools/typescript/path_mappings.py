# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import sys
import json
import io
import os
from path_utils import isInAshFolder, getTargetPath

def _add_ui_webui_resources_mappings(path_mappings, root_gen_dir):
  # Calculate mappings for ui/webui/resources/ sub-folders that have a dedicated
  # ts_library() target. The naming of the ts_library() target is expected to
  # follow the default "build_ts" naming in the build_webui() rule. The output
  # folder is expected to be at '$root_gen_dir/ui/webui/resources/tsc/'.
  shared_ts_folders = [
      "cr_elements",
      "js",
      "mojo",
      "cr_components/app_management",
      "cr_components/certificate_manager",
      "cr_components/color_change_listener",
      "cr_components/commerce",
      "cr_components/customize_color_scheme_mode",
      "cr_components/customize_themes",
      "cr_components/help_bubble",
      "cr_components/history",
      "cr_components/history_embeddings",
      "cr_components/history_clusters",
      "cr_components/localized_link",
      "cr_components/managed_dialog",
      "cr_components/managed_footnote",
      "cr_components/most_visited",
      "cr_components/page_image_service",
      "cr_components/searchbox",
      "cr_components/settings_prefs",
      "cr_components/theme_color_picker",
  ]

  for c in shared_ts_folders:
    path_mappings[f'//ui/webui/resources/{c}:build_ts'] = [(
        f'//resources/{c}/*',
        f'{root_gen_dir}/ui/webui/resources/tsc/{c}/*',
    )]


def _add_third_party_polymer_mappings(path_mappings, root_src_dir):
  path_mappings[f'//third_party/polymer/v3_0:library'] = [
      ('//resources/polymer/v3_0/polymer/polymer_bundled.min.js',
       f'{root_src_dir}/third_party/polymer/v3_0/components-chromium/polymer/polymer.d.ts'
       ),
      ('//resources/polymer/v3_0/*',
       f'{root_src_dir}/third_party/polymer/v3_0/components-chromium/*')
  ]


def _add_third_party_lit_mappings(path_mappings, root_gen_dir):
  path_mappings[f'//third_party/lit/v3_0:build_ts'] = [
      ('//resources/lit/v3_0/lit.rollup.js',
       f'{root_gen_dir}/third_party/lit/v3_0/lit.d.ts'),
  ]


# Ash-only
def _add_ash_mappings(path_mappings, root_gen_dir, root_src_dir):
  # Note: The path for this target shadows all the paths for |shared_ts_folders|
  # below. Eventually this target should be removed and everything should reside
  # in a subfolder, so that missing deps can surface during the build, similar
  # to how ui/webui/resources/ works.
  path_mappings['//ash/webui/common/resources:build_ts'] = [(
      '//resources/ash/common/*',
      f'{root_gen_dir}/ash/webui/common/resources/preprocessed/*',
  )]

  # Calculate mappings for ash/webui/common/resources/ sub-folders that have a
  # dedicated ts_library() target. The naming of the ts_library() target is
  # expected to follow the default "build_ts" naming in the build_webui() rule.
  # The output folder is expected to be at
  # '$root_gen_dir/ash/webui/common/resources/preprocessed/'.
  shared_ts_folders = [
      "cellular_setup",
      "cr_elements",
      "personalization",
      "sea_pen",

      # List more folders here as they get migrated to use build_webui().
  ]

  for c in shared_ts_folders:
    path_mappings[f'//ash/webui/common/resources/{c}:build_ts'] = [(
        f'//resources/ash/common/{c}/*',
        f'{root_gen_dir}/ash/webui/common/resources/preprocessed/{c}/*',
    )]

  path_mappings['//third_party/cros-components:cros_components_ts'] = [(
      '//resources/cros_components/*',
      f'{root_gen_dir}/ui/webui/resources/tsc/cros_components/to_be_rewritten/*',
  )]
  path_mappings['//third_party/material_web_components:library'] = [(
      '//resources/mwc/@material/*',
      f'{root_src_dir}/third_party/material_web_components/components-chromium/'
      'node_modules/@material/*',
  )]
  path_mappings['//third_party/material_web_components:bundle_lit_ts'] = [
      ('//resources/mwc/lit/index.js',
       f'{root_src_dir}/third_party/material_web_components/lit_exports.d.ts')
  ]


def GetDepToPathMappings(root_gen_dir, root_src_dir, platform):
  path_mappings = {}

  _add_ui_webui_resources_mappings(path_mappings, root_gen_dir)
  _add_third_party_polymer_mappings(path_mappings, root_src_dir)
  _add_third_party_lit_mappings(path_mappings, root_gen_dir)

  if platform == 'chromeos_ash':
    _add_ash_mappings(path_mappings, root_gen_dir, root_src_dir)

  return path_mappings


def _is_browser_only_dep(dep):
  browser_only_deps = [
      '//ui/webui/resources/cr_elements',
      '//ui/webui/resources/cr_components/localized_link',
      '//ui/webui/resources/cr_components/managed_footnote',
  ]
  return any(dep.startswith(dep_folder) for dep_folder in browser_only_deps)


def _is_dependency_allowed(is_ash_target, raw_dep, target_path):
  if is_ash_target and _is_browser_only_dep(raw_dep):
    return False

  is_ash_dep = isInAshFolder(raw_dep[2:])
  if not is_ash_dep or is_ash_target:
    return True

  exceptions = [
      # TODO(crbug.com/40946949): Remove this incorrect dependency
      'chrome/browser/resources/settings',
  ]

  return target_path in exceptions


def _write_path_mappings_file(path_mappings, output_suffix, out_dir,
                              pretty_print):
  path_mappings_filename = f'path_mappings_{output_suffix}.json'
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  out_file_path = os.path.join(out_dir, path_mappings_filename)
  with open(out_file_path, 'w', encoding='utf-8') as map_file:
    indent = 2 if pretty_print else None
    map_file.write(json.dumps(path_mappings, indent=indent))


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--raw_deps', nargs='*')
  parser.add_argument('--root_gen_dir', required=True)
  parser.add_argument('--root_src_dir', required=True)
  parser.add_argument('--gen_dir', required=True)
  parser.add_argument('--output_suffix', required=True)
  parser.add_argument(
      '--webui_context_type',
      choices=['trusted', 'untrusted', 'relative', 'trusted_only'],
      default='trusted')
  parser.add_argument('--pretty_print', action='store_true')
  parser.add_argument('--platform',
                      choices=['other', 'ios', 'chromeos_ash'],
                      default='other')
  args = parser.parse_args(argv)

  dep_to_path_mappings = GetDepToPathMappings(
      args.root_gen_dir,
      # Sometimes root_src_dir has trailing slashes. Remove them if necessary.
      args.root_src_dir.rstrip('/'),
      args.platform)

  target_path = getTargetPath(args.gen_dir, args.root_gen_dir)
  is_ash_target = isInAshFolder(target_path)
  path_mappings = collections.defaultdict(list)
  for dep in args.raw_deps:
    dependencyType = 'Browser-only' if is_ash_target else 'Ash-only'
    assert _is_dependency_allowed(is_ash_target, dep, target_path), \
        f'{target_path} should not use {dependencyType} dependency {dep}.'

    if dep not in dep_to_path_mappings:
      assert not dep.startswith("//ui/webui/resources"), \
          f'Missing path mapping for \'{dep}\'. Update ' \
          '//tools/typescript/path_mappings.py accordingly.'

      # Path mappings outside of //ui/webui/resources are not inferred from
      # |args.deps| yet.
      continue

    mappings = dep_to_path_mappings[dep]
    scheme = \
        'chrome-untrusted:' if args.webui_context_type == 'untrusted' else 'chrome:'
    for (url, dir) in mappings:
      if (args.webui_context_type != 'trusted_only'):
        path_mappings[url].append(os.path.join('./', dir).replace('\\', '/'))
      if (url.startswith("//") and args.webui_context_type != 'relative'):
        path_mappings[scheme + url].append(
            os.path.join('./', dir).replace('\\', '/'))

  _write_path_mappings_file(path_mappings, args.output_suffix, args.gen_dir,
                            args.pretty_print)


if __name__ == '__main__':
  main(sys.argv[1:])
