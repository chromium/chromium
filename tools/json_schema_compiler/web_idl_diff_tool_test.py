#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import web_idl_diff_tool

# Test to ensure there are no difference in parsing + processing extension API
# schemas when converting them to be written using WebIDL. Uses the alarms API
# schema files as an example to test, copied over at the time they were
# initially converted to WebIDL.


class WebIdlDiffToolTest(unittest.TestCase):

  def testIdlToWebIdlConversion(self):
    # Note: the following schemas were copied over at the time of their
    # conversion and are not intended to be kept up to date with any more recent
    # updates. Once all the old IDL has been converted and we are ready to
    # remove the old parser, these can all be deleted and this test removed.
    converted_schemas = [
        ('alarms.idl', 'alarms.webidl'),
        ('app_current_window_internal.idl',
         'app_current_window_internal.webidl'),
        ('bluetooth.idl', 'bluetooth.webidl'),
        ('dns.idl', 'dns.webidl'),
        ('audio.idl', 'audio.webidl'),
        ('cec_private.idl', 'cec_private.webidl'),
        ('diagnostics.idl', 'diagnostics.webidl'),
        ('virtual_keyboard.idl', 'virtual_keyboard.webidl'),
        ('webcam_private.idl', 'webcam_private.webidl'),
        ('extension_options_internal.idl', 'extension_options_internal.webidl'),
        ('system_cpu.idl', 'system_cpu.webidl'),
        ('system_memory.idl', 'system_memory.webidl'),
        ('system_network.idl', 'system_network.webidl'),
        ('system_storage.idl', 'system_storage.webidl'),
        ('app_runtime.idl', 'app_runtime.webidl'),
        ('bluetooth_low_energy.idl', 'bluetooth_low_energy.webidl'),
        ('bluetooth_socket.idl', 'bluetooth_socket.webidl'),
        ('clipboard.idl', 'clipboard.webidl'),
        ('system_display.idl', 'system_display.webidl'),
        ('usb.idl', 'usb.webidl'),
        ('chrome_url_overrides.idl', 'chrome_url_overrides.webidl'),
        ('cross_origin_isolation.idl', 'cross_origin_isolation.webidl'),
        ('file_handlers.idl', 'file_handlers.webidl'),
        ('oauth2.idl', 'oauth2.webidl'),
        ('protocol_handlers.idl', 'protocol_handlers.webidl'),
        ('shared_module.idl', 'shared_module.webidl'),
        ('web_accessible_resources.idl', 'web_accessible_resources.webidl'),
        ('web_accessible_resources_mv2.idl',
         'web_accessible_resources_mv2.webidl'),
        ('automation_internal.idl', 'automation_internal.webidl'),
        ('feedback_private.idl', 'feedback_private.webidl'),
        ('media_perception_private.idl', 'media_perception_private.webidl'),
        ('mojo_private.idl', 'mojo_private.webidl'),
        ('networking_onc.idl', 'networking_onc.webidl'),
        ('networking_private.idl', 'networking_private.webidl'),
        ('offscreen.idl', 'offscreen.webidl'),
        ('power.idl', 'power.webidl'),
        ('serial.idl', 'serial.webidl'),
        ('bluetooth_private.idl', 'bluetooth_private.webidl'),
        ('content_scripts.idl', 'content_scripts.webidl'),
    ]
    # LoadAndReturnUnifiedDiff expects file paths relative to the repo root.
    converted_schema_path = 'tools/json_schema_compiler/test/converted_schemas/'
    for old_schema_name, new_schema_name in converted_schemas:
      old_filename = converted_schema_path + old_schema_name
      new_filename = converted_schema_path + new_schema_name
      diff = web_idl_diff_tool.LoadAndReturnUnifiedDiff(old_filename,
                                                        new_filename)
      self.assertEqual(
          '',
          diff,
          f"Difference detected between '{old_filename}' and"
          f" '{new_filename}':\n{diff}",
      )


if __name__ == '__main__':
  unittest.main()
