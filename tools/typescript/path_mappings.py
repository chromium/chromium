# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path


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
      "cr_components/customize_themes",
      "cr_components/help_bubble",
      "cr_components/history_clusters",
      "cr_components/localized_link",
      "cr_components/managed_dialog",
      "cr_components/managed_footnote",
      "cr_components/most_visited",
      "cr_components/omnibox",
      "cr_components/page_image_service",
      "cr_components/settings_prefs",
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


# Ash-only
def _add_ash_mappings(path_mappings, root_gen_dir, root_src_dir):
  path_mappings['//ash/webui/common/resources:build_ts'] = [(
      '//resources/ash/common/*',
      f'{root_gen_dir}/ash/webui/common/resources/preprocessed/*',
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

  if platform == 'chromeos_ash':
    _add_ash_mappings(path_mappings, root_gen_dir, root_src_dir)

  return path_mappings
