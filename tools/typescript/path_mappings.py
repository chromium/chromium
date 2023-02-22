# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetDepToPathMappings(root_gen_dir):
  # Calculate mappings for ui/webui/resources/ sub-folders that have a dedicated
  # ts_library() target. The naming and output folder of the ts_library target
  # is assumed to follow the defaults in the build_cr_component() rule.
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
      "cr_components/image_service",
      "cr_components/localized_link",
      "cr_components/managed_dialog",
      "cr_components/managed_footnote",
      "cr_components/most_visited",
      "cr_components/omnibox",
  ]

  path_mappings = {}
  for c in shared_ts_folders:
    # TODO(dpapad): Rename //ui/webui/resources/mojo:library to
    # //ui/webui/resources/mojo:build_ts and remove this special casing.
    target_name = 'library' if c == "mojo" else 'build_ts'

    path_mappings[f'//ui/webui/resources/{c}:{target_name}'] = [(
        f'//resources/{c}/*',
        f'{root_gen_dir}/ui/webui/resources/tsc/{c}/*',
    )]

  return path_mappings
