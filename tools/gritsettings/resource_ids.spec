# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is used to assign starting resource ids for resources and strings
# used by Chromium.  This is done to ensure that resource ids are unique
# across all the grd files.  If you are adding a new grd file, please add
# a new entry to this file.
#
# The entries below are organized into sections. When adding new entries,
# please use the right section. Try to keep entries in alphabetical order.
#
# - chrome/app/
# - chrome/browser/
# - chrome/ WebUI
# - chrome/ miscellaneous
# - chromeos/
# - components/
# - ios/ (overlaps with chrome/)
# - content/
# - ios/web/ (overlaps with content/)
# - everything else
#
# The range of ID values, which is used by pak files, is from 0 to 2^16 - 1.
#
# IMPORTANT: Update instructions:
# * If adding items, manually assign draft start IDs so that numerical order is
#   preserved. Usually it suffices to +1 from previous tag.
#   * If updating items with repeated, be sure to add / update
#     "META": {"join": <duplicate count>},
#     for the item following duplicates. Be sure to look for duplicates that
#     may appear earlier than those that immediately precede the item.
{
  # The first entry in the file, SRCDIR, is special: It is a relative path from
  # this file to the base of your checkout.
  "SRCDIR": "../..",

  # START chrome/app section.
  # Previous versions of this file started with resource id 400, so stick with
  # that.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "chrome/app/chromium_strings.grd": {
    "messages": [400],
  },
  "chrome/app/google_chrome_strings.grd": {
    "messages": [400],
  },

  # Leave lots of space for generated_resources since it has most of our
  # strings.
  "chrome/app/generated_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [600],
  },

  "chrome/app/resources/locale_settings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 1000},
    "messages": [1000],
  },

  # These each start with the same resource id because we only use one
  # file for each build (chromiumos, google_chromeos, linux, mac, or win).
  "chrome/app/resources/locale_settings_chromiumos.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_google_chromeos.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_linux.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_mac.grd": {
    "messages": [1100],
  },
  "chrome/app/resources/locale_settings_win.grd": {
    "messages": [1100],
  },

  "chrome/app/theme/chrome_unscaled_resources.grd": {
    "META": {"join": 5},
    "includes": [1120],
  },

  # Leave space for theme_resources since it has many structures.
  "chrome/app/theme/theme_resources.grd": {
    "structures": [1140],
  },
  # END chrome/app section.

  # START chrome/browser section.
  "chrome/browser/dev_ui_browser_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [1200],
  },
  "chrome/browser/browser_resources.grd": {
    "includes": [1220],
    "structures": [1240],
  },
  "chrome/browser/resources/bookmarks/bookmarks_resources.grd": {
    "includes": [1260],
    "structures": [1280],
  },
  "chrome/browser/resources/bookmarks/bookmarks_resources_vulcanized.grd": {
    "includes": [1300],
  },
  "chrome/browser/resources/chromeos/cellular_setup/cellular_setup_resources.grd": {
    "structures": [1360],
  },
  "chrome/browser/resources/chromeos/multidevice_internals/multidevice_internals_resources.grd": {
    "includes": [1370],
    "structures": [1380],
  },
  "chrome/browser/resources/chromeos/multidevice_setup/multidevice_setup_resources.grd": {
    "structures": [1400],
  },
  "chrome/browser/resources/component_extension_resources.grd": {
    "includes": [1420],
    "structures": [1440],
  },
  "chrome/browser/resources/downloads/downloads_resources_vulcanized.grd": {
    "includes": [1460],
  },
  "chrome/browser/resources/downloads/downloads_resources.grd": {
    "includes": [1480],
    "structures": [1500],
  },
  "chrome/browser/resources/extensions/extensions_resources_vulcanized.grd": {
    "includes": [1520],
  },
  "chrome/browser/resources/extensions/extensions_resources.grd": {
    "includes": [1540],
    "structures": [1560],
  },
  "chrome/browser/resources/history/history_resources_vulcanized.grd": {
    "includes": [1580],
  },
  "chrome/browser/resources/history/history_resources.grd": {
    "includes": [1600],
  },
  "chrome/browser/resources/local_ntp/local_ntp_resources.grd": {
    "includes": [1620],
  },
  "chrome/browser/resources/nearby_internals/nearby_internals_resources.grd": {
    "includes": [1630],
  },
  "chrome/browser/resources/nearby_share/nearby_share_dialog_resources.grd": {
    "includes": [1640],
  },
  "chrome/browser/resources/nearby_share/shared/nearby_shared_resources.grd": {
    "includes": [1645],
  },
  "chrome/browser/resources/nearby_share/shared/nearby_shared_resources_v3.grd": {
    "includes": [1650],
  },
  "chrome/browser/resources/new_tab_page/new_tab_page_resources_vulcanized.grd": {
    "includes": [1660],
  },
  "chrome/browser/resources/new_tab_page/new_tab_page_resources.grd": {
    "includes": [1680],
  },
  "chrome/browser/resources/print_preview/print_preview_resources_vulcanized.grd": {
    "includes": [1700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/print_preview/print_preview_resources.grd": {
    "META": {"sizes": {"includes": [500],}},
    "includes": [1720],
  },
  "chrome/browser/resources/print_preview/print_preview_pdf_resources.grd": {
    "includes": [1750],
  },
  "chrome/browser/resources/read_later/read_later_resources.grd": {
    "includes": [1760],
  },
  "chrome/browser/resources/settings/os_settings_resources_vulcanized.grd": {
    "includes": [1770],
  },
  "chrome/browser/resources/settings/os_settings_resources.grd": {
    "includes": [1780],
    "structures": [1800],
  },
  "chrome/browser/resources/settings/settings_resources_vulcanized.grd": {
    "includes": [1820],
  },
  "chrome/browser/resources/settings/settings_resources.grd": {
    "includes": [1830],
    "structures": [1840],
  },
  "chrome/browser/resources/signin/profile_picker/profile_picker_resources_vulcanized.grd": {
    "includes": [1850],
  },
 "chrome/browser/resources/signin/profile_picker/profile_picker_resources.grd": {
    "includes": [1860],
    "structures": [1870],
  },
  "chrome/browser/resources/tab_search/tab_search_resources.grd": {
    "includes": [1880],
  },
  "chrome/browser/resources/tab_strip/tab_strip_resources.grd": {
    "structures": [1900],
    "includes": [1920],
  },
  "chrome/browser/resources/welcome/welcome_resources.grd": {
    "includes": [1940],
    "structures": [1960],
  },
  "chrome/browser/supervised_user/supervised_user_unscaled_resources.grd": {
    "includes": [1970],
  },
  "chrome/browser/test_dummy/internal/android/resources/resources.grd": {
    "includes": [1980],
  },
  # END chrome/browser section.

  # START chrome/ WebUI resources section
  # Both the kaleidoscope_resources.grd and kaleidoscope_internal_resources.grd
  # start with the same id because only one of them is built based on whether
  # src-internal is available.
  "chrome/browser/media/kaleidoscope/kaleidoscope_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2000],
  },
  "chrome/browser/media/kaleidoscope/kaleidoscope_internal_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2000],
  },
  # The internal version of kaleidoscope_resources.grd will be removed in a
  # follow up. It is only here to avoid build breakages.
  "chrome/browser/media/kaleidoscope/internal/kaleidoscope_resources.grd": {
    "META": {"sizes": {"includes": [50],}},  # Relies on src-internal.
    "includes": [2010],
  },
  "chrome/browser/media/kaleidoscope/internal/kaleidoscope_strings.grd": {
    "META": {"sizes": {"messages": [50]}, "join": 2},  # Relies on src-internal.
    "messages": [2015],
  },
  "chrome/browser/resources/bluetooth_internals/resources.grd": {
    "includes": [2020],
  },
    "chrome/browser/resources/chromeos/bluetooth_pairing_dialog/bluetooth_pairing_dialog_resources.grd": {
    "includes": [2030],
    "structures": [2050],
  },
  "chrome/browser/resources/chromeos/bluetooth_pairing_dialog/bluetooth_pairing_dialog_resources_vulcanized.grd": {
    "includes": [2070],
  },
  "chrome/browser/resources/gaia_auth_host/gaia_auth_host_resources.grd": {
    "includes": [2080],
  },
  "chrome/browser/resources/invalidations/invalidations_resources.grd": {
    "includes": [2090],
  },
  "chrome/browser/resources/media/webrtc_logs_resources.grd": {
    "includes": [2100],
  },
  "chrome/browser/resources/net_internals/net_internals_resources.grd": {
    "includes": [2120],
  },
  "chrome/browser/resources/omnibox/resources.grd": {
    "includes": [2140],
  },
  "chrome/browser/resources/quota_internals/quota_internals_resources.grd": {
    "includes": [2160],
  },
  "chrome/browser/resources/sync_file_system_internals/sync_file_system_internals_resources.grd": {
    "includes": [2180],
  },
  "chrome/browser/resources/usb_internals/resources.grd": {
    "includes": [2200],
  },
  "chrome/browser/resources/webapks/webapks_ui_resources.grd": {
    "includes": [2220],
  },
  "components/sync/driver/resources.grd": {
    "includes": [2240],
  },
  "components/resources/dev_ui_components_resources.grd": {
    "includes": [2260],
  },
  "content/browser/resources/media/media_internals_resources.grd": {
    "includes": [2270],
  },
  "content/browser/webrtc/resources/resources.grd": {
    "includes": [2280],
  },
  "content/dev_ui_content_resources.grd": {
    "includes": [2300],
  },
  # END chrome/ WebUI resources section

  # START chrome/ miscellaneous section.
  "chrome/common/common_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2320],
  },
  "chrome/credential_provider/gaiacp/gaia_resources.grd": {
    "includes": [2340],
    "messages": [2360],
  },
  "chrome/renderer/resources/renderer_resources.grd": {
    "includes": [2380],
    "structures": [2400],
  },
  "chrome/test/data/webui_test_resources.grd": {
    "includes": [2420],
  },
  # END chrome/ miscellaneous section.

  # START chromeos/ section.
  "chromeos/chromeos_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [2500],
  },
  "chromeos/components/camera_app_ui/resources/camera_app_resources.grd": {
    "includes": [2505],
    "structures": [2510],
  },
  "chromeos/components/camera_app_ui/resources/strings/camera_strings.grd": {
    "messages": [2515],
  },
  "chromeos/components/diagnostics_ui/resources/diagnostics_app_resources.grd": {
    "includes": [2517],
  },
  "chromeos/components/file_manager/resources/file_manager_resources.grd": {
    "includes": [2518],
  },
  "chromeos/components/help_app_ui/resources/help_app_resources.grd": {
    "includes": [2520],
  },
  # Both help_app_bundle_resources.grd and help_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound is that we bundle ~100 images for
  # offline articles with the app, as well as strings in every language (74),
  # and bundled content in the top 25 languages (25 x 2).
  "chromeos/components/help_app_ui/resources/prod/help_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [300],}},  # Relies on src-internal.
    "includes": [2540],
  },
  "chromeos/components/help_app_ui/resources/mock/help_app_bundle_mock_resources.grd": {
    "includes": [2540],
  },
  "chromeos/components/media_app_ui/resources/media_app_resources.grd": {
    "META": {"join": 2},
    "includes": [2560],
  },
  # Both media_app_bundle_resources.grd and media_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound for number of resource ids is number
  # of languages (74).
  "chromeos/components/media_app_ui/resources/prod/media_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [120],}},  # Relies on src-internal.
    "includes": [2580],
  },
  "chromeos/components/media_app_ui/resources/mock/media_app_bundle_mock_resources.grd": {
    "includes": [2580],
  },
  "chromeos/components/print_management/resources/print_management_resources.grd": {
    "META": {"join": 2},
    "includes": [2600],
    "structures": [2620],
  },
  "chromeos/components/sample_system_web_app_ui/resources/sample_system_web_app_resources.grd": {
    "includes": [2640],
  },
  "chromeos/components/scanning/resources/scanning_app_resources.grd": {
    "includes": [2645],
    "structures": [2650],
  },
  "chromeos/components/telemetry_extension_ui/resources/telemetry_extension_resources.grd": {
    "includes": [2655],
  },
  "chromeos/resources/chromeos_resources.grd": {
    "includes": [2660],
  },
  # END chromeos/ section.

  # START components/ section.
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "components/components_chromium_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [2700],
  },
  "components/components_google_chrome_strings.grd": {
    "messages": [2700],
  },

  "components/components_locale_settings.grd": {
    "META": {"join": 2},
    "includes": [2720],
    "messages": [2740],
  },
  "components/components_strings.grd": {
    "messages": [2760],
  },
  "components/omnibox/resources/omnibox_resources.grd": {
    "includes": [2780],
  },
  "components/policy/resources/policy_templates.grd": {
    "structures": [2800],
  },
  "components/resources/components_resources.grd": {
    "includes": [2820],
  },
  "components/resources/components_scaled_resources.grd": {
    "structures": [2840],
  },
  "components/embedder_support/android/java/strings/web_contents_delegate_android_strings.grd": {
    "messages": [2860],
  },
  "components/autofill/core/browser/autofill_address_rewriter_resources.grd":{
    "includes": [2880]
  },
  # END components/ section.

  # START ios/ section.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "ios/chrome/app/resources/ios_resources.grd": {
    "includes": [400],
    "structures": [420],
  },

  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "ios/chrome/app/strings/ios_chromium_strings.grd": {
    # Big alignment to make start IDs look nicer.
    "META": {"align": 100},
    "messages": [500],
  },
  "ios/chrome/app/strings/ios_google_chrome_strings.grd": {
    "messages": [500],
  },

  "ios/chrome/app/strings/ios_strings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [600],
  },
  "ios/chrome/app/theme/ios_theme_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "structures": [700],
  },
  "ios/chrome/share_extension/strings/ios_share_extension_strings.grd": {
    "messages": [720],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_strings.grd": {
    "messages": [740],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_chromium_strings.grd": {
    "messages": [760],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_google_chrome_strings.grd": {
    "messages": [760],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_chromium_strings.grd": {
    "META": {"join": 2},
    "messages": [780],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_google_chrome_strings.grd": {
    "messages": [780],
  },
  "ios/chrome/credential_provider_extension/strings/ios_credential_provider_extension_strings.grd": {
    "META": {"join": 2},
    "messages": [800],
  },

  # END ios/ section.

  # START content/ section.
  # content/ and ios/web/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "content/app/resources/content_resources.grd": {
    # Big alignment at start of section.
    "META": {"join": 2, "align": 100},
    "structures": [2900],
  },
  "content/content_resources.grd": {
    "includes": [2920],
  },
  "content/shell/shell_resources.grd": {
    "includes": [2940],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [2960],
  },
  # END content/ section.

  # START ios/web/ section.
  # content/ and ios/web/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "ios/web/ios_web_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2900],
  },
  "ios/web/test/test_resources.grd": {
    "includes": [2920],
  },
  # END ios/web/ section.

  # START "everything else" section.
  # Everything but chrome/, chromeos/, components/, content/, and ios/
  "android_webview/ui/aw_resources.grd": {
    # Big alignment at start of section.
    "META": {"join": 2, "align": 100},
    "includes": [3000],
  },
  "android_webview/ui/aw_strings.grd": {
    "messages": [3020],
  },

  "ash/app_list/resources/app_list_resources.grd": {
    "structures": [3040],
  },
  "ash/ash_strings.grd": {
    "messages": [3060],
  },
  "ash/shortcut_viewer/shortcut_viewer_strings.grd": {
    "messages": [3080],
  },
  "ash/keyboard/ui/keyboard_resources.grd": {
    "includes": [3100],
  },
  "ash/login/resources/login_resources.grd": {
    "structures": [3120],
  },
  "ash/public/cpp/resources/ash_public_unscaled_resources.grd": {
    "includes": [3140],
  },
  "base/tracing/protos/resources.grd": {
    "includes": [3150],
  },
  "chromecast/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [3160],
  },

  "cloud_print/virtual_driver/win/install/virtual_driver_setup_resources.grd": {
    "includes": [3180],
    "messages": [3200],
  },

  "device/bluetooth/bluetooth_strings.grd": {
    "messages": [3220],
  },

  "device/fido/fido_strings.grd": {
    "messages": [3240],
  },

  "extensions/browser/resources/extensions_browser_resources.grd": {
    "structures": [3260],
  },
  "extensions/extensions_resources.grd": {
    "includes": [3280],
  },
  "extensions/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [3300],
    "structures": [3320],
  },
  "extensions/shell/app_shell_resources.grd": {
    "includes": [3340],
  },
  "extensions/strings/extensions_strings.grd": {
    "messages": [3360],
  },

  "headless/lib/resources/headless_lib_resources.grd": {
    "includes": [3380],
  },

  "mojo/public/js/mojo_bindings_resources.grd": {
    "includes": [3400],
  },

  "net/base/net_resources.grd": {
    "includes": [3420],
  },

  "remoting/resources/remoting_strings.grd": {
    "messages": [3440],
  },

  "services/services_strings.grd": {
    "messages": [3460],
  },
  "skia/skia_resources.grd": {
    "includes": [3470],
  },
  "third_party/blink/public/blink_image_resources.grd": {
    "structures": [3480],
  },
  "third_party/blink/public/blink_resources.grd": {
    "includes": [3500],
  },
  "third_party/blink/renderer/modules/media_controls/resources/media_controls_resources.grd": {
    "includes": [3520],
    "structures": [3540],
  },
  "third_party/blink/public/strings/blink_strings.grd": {
    "messages": [3560],
  },
  "third_party/ink/ink_resources.grd": {
    "includes": [3580],
  },
  "third_party/libaddressinput/chromium/address_input_strings.grd": {
    "messages": [3600],
  },

  "ui/base/test/ui_base_test_resources.grd": {
    "messages": [3620],
  },
  "ui/chromeos/resources/ui_chromeos_resources.grd": {
    "structures": [3640],
  },
  "ui/chromeos/ui_chromeos_strings.grd": {
    "messages": [3660],
  },
  "ui/file_manager/file_manager_resources.grd": {
    "includes": [3680],
  },
  "ui/resources/ui_resources.grd": {
    "structures": [3700],
  },
  "ui/resources/ui_unscaled_resources.grd": {
    "includes": [3720],
  },
  "ui/strings/app_locale_settings.grd": {
    "messages": [3740],
  },
  "ui/strings/ui_strings.grd": {
    "messages": [3760],
  },
  "ui/views/examples/views_examples_resources.grd": {
    "messages": [3770],
  },
  "ui/views/resources/views_resources.grd": {
    "structures": [3780],
  },
  "ui/webui/resources/webui_resources.grd": {
    "includes": [3800],
    "structures": [3820],
  },
  "weblayer/weblayer_resources.grd": {
    "includes": [3840],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd": {
    "META": {"sizes": {"includes": [1000],}},
    "includes": [3860],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/resources/inspector_overlay/inspector_overlay_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3880],
  },

  # END "everything else" section.
  # Everything but chrome/, components/, content/, and ios/

  # Thinking about appending to the end?
  # Please read the header and find the right section above instead.

  # Resource ids starting at 31000 are reserved for projects built on Chromium.
}
