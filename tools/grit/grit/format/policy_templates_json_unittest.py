#!/usr/bin/env python
# coding: utf-8
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for policy_templates_json.py.
"""

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import grit.extern.tclib
import tempfile
import unittest

from six import StringIO

from grit import grd_reader
from grit.tool import build


class PolicyTemplatesJsonUnittest(unittest.TestCase):

  def testPolicyTranslation(self):
    # Create test policy_templates.json data.
    caption = "The main policy"
    caption_translation = "Die Hauptrichtlinie"

    message = \
      "Red cabbage stays red cabbage and wedding dress stays wedding dress"
    message_translation = \
      "Blaukraut bleibt Blaukraut und Brautkleid bleibt Brautkleid"

    schema_key_description = "Number of users"
    schema_key_description_translation = "Anzahl der Nutzer"

    policy_json = """
        {
          "policy_definitions": [
            {
              'name': 'MainPolicy',
              'type': 'main',
              'owners': ['foo@bar.com'],
              'schema': {
                'properties': {
                  'default_launch_container': {
                    'enum': [
                      'tab',
                      'window',
                    ],
                    'type': 'string',
                  },
                  'users_number': {
                    'description': '''%s''',
                    'type': 'integer',
                  },
                },
                'type': 'object',
              },
              'supported_on': ['chrome_os:29-'],
              'features': {
                'can_be_recommended': True,
                'dynamic_refresh': True,
              },
              'example_value': True,
              'caption': '''%s''',
              'tags': [],
              'desc': '''This policy does stuff.'''
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {
            'message_string_id': {
              'desc': '''The description is removed from the grit output''',
              'text': '''%s'''
            }
          }
        }""" % (schema_key_description, caption, message)

    # Create translations. The translation IDs are hashed from the English text.
    caption_id = grit.extern.tclib.GenerateMessageId(caption);
    message_id = grit.extern.tclib.GenerateMessageId(message);
    schema_key_description_id = grit.extern.tclib.GenerateMessageId(
        schema_key_description)
    policy_xtb = """
<?xml version="1.0" ?>
<!DOCTYPE translationbundle>
<translationbundle lang="de">
<translation id="%s">%s</translation>
<translation id="%s">%s</translation>
<translation id="%s">%s</translation>
</translationbundle>""" % (caption_id, caption_translation,
                           message_id, message_translation,
                           schema_key_description_id,
                           schema_key_description_translation)

    # Write both to a temp file.
    tmp_dir_name = tempfile.gettempdir()

    json_file_path = os.path.join(tmp_dir_name, 'test.json')
    with open(json_file_path, 'w') as f:
      f.write(policy_json.strip())

    xtb_file_path = os.path.join(tmp_dir_name, 'test.xtb')
    with open(xtb_file_path, 'w') as f:
      f.write(policy_xtb.strip())

    # Assemble a test grit tree, similar to policy_templates.grd.
    grd_text = '''
    <grit base_dir="." latest_public_release="0" current_release="1" source_lang_id="en">
      <translations>
        <file path="%s" lang="de" />
      </translations>
      <release seq="1">
        <structures>
          <structure name="IDD_POLICY_SOURCE_FILE" file="%s" type="policy_template_metafile" />
        </structures>
      </release>
    </grit>''' % (xtb_file_path, json_file_path)
    grd_string_io = StringIO(grd_text)

    # Parse the grit tree and load the policies' JSON with a gatherer.
    grd = grd_reader.Parse(grd_string_io, dir=tmp_dir_name, defines={'_google_chrome': True})
    grd.SetOutputLanguage('en')
    grd.RunGatherers()

    # Remove the temp files.
    os.unlink(xtb_file_path)
    os.unlink(json_file_path)

    # Run grit with en->de translation.
    env_lang = 'en'
    out_lang = 'de'
    env_defs = {'_google_chrome': '1'}

    grd.SetOutputLanguage(env_lang)
    grd.SetDefines(env_defs)
    buf = StringIO()
    build.RcBuilder.ProcessNode(grd, DummyOutput('policy_templates', out_lang), buf)
    output = buf.getvalue()

    # Caption and message texts get taken from xtb.
    # desc is 'translated' to some pseudo-English
    #   'ThïPïs pôPôlïPïcýPý dôéPôés stüPüff'.
    expected = u"""{
  "policy_definitions": [
    {
      "caption": "%s",
      "desc": "Th\xefP\xefs p\xf4P\xf4l\xefP\xefc\xfdP\xfd d\xf4\xe9P\xf4\xe9s st\xfcP\xfcff.",
      "example_value": true,
      "features": {"can_be_recommended": true, "dynamic_refresh": true},
      "name": "MainPolicy",
      "owners": ["foo@bar.com"],
      "schema": {
        "properties": {
          "default_launch_container": {
            "enum": [
              "tab",
              "window"
            ],
            "type": "string"
          },
          "users_number": {
            "description": "%s",
            "type": "integer"
          }
        },
        "type": "object"
      },
      "supported_on": ["chrome_os:29-"],
      "tags": [],
      "type": "main"
    }
  ],
  "policy_atomic_group_definitions": [
  ],
  "messages": {
    "message_string_id": {
      "text": "%s"
    }
  }

}""" % (caption_translation, schema_key_description_translation,
        message_translation)
    self.assertEqual(expected, output)


class DummyOutput(object):

  def __init__(self, type, language):
    self.type = type
    self.language = language

  def GetType(self):
    return self.type

  def GetLanguage(self):
    return self.language

  def GetOutputFilename(self):
    return 'hello.gif'
