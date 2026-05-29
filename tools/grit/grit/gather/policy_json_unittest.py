#!/usr/bin/env python3
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.gather.policy_json'''

import io
import json
import os
import re
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.gather import policy_json

class PolicyJsonUnittest(unittest.TestCase):

  def GetExpectedOutput(self, original):
    expected = original.copy()
    for key, message in expected['messages'].items():
      del message['desc']
    return expected

  def testEmpty(self):
    original = {
        'policy_definitions': [],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 0)
    self.assertTrue(json.dumps(original) == json.dumps(
            json.loads(gatherer.Translate('en'))))

  def testGeneralPolicy(self):
    original = {
        'policy_definitions': [
            {
                'name': 'HomepageLocation',
                'type': 'string',
                'owners': ['foo@bar.com'],
                'supported_on': ['chrome.*:8-'],
                'features': {
                    'dynamic_refresh': 1
                },
                'example_value': 'http://chromium.org',
                'caption': 'nothing special 1',
                'desc': 'nothing special 2',
                'label': 'nothing special 3',
            },
        ],
        'policy_atomic_group_definitions': [],
        'messages': {
            'msg_identifier': {
                'text': 'nothing special 3',
                'desc': 'nothing special descr 3',
            }
        }
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 4)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testEnum(self):
    original = {
        'policy_definitions': [
            {
                'name': 'Policy1',
                'owners': ['a@b'],
                'items': [{
                    'name': 'Item1',
                    'caption': 'nothing special',
                }]
            },
        ],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testSchema(self):
    original = {
        'policy_definitions': [
            {
                'name': 'Policy1',
                'schema': {
                    'type': 'object',
                    'properties': {
                        'outer': {
                            'description': 'outer description',
                            'type': 'object',
                            'inner': {
                                'description': 'inner description',
                                'type': 'integer',
                                'minimum': 0,
                                'maximum': 100
                            },
                            'inner2': {
                                'description': 'inner2 description',
                                'type': 'integer',
                                'enum': [1, 2, 3],
                                'sensitiveValue': True
                            },
                        },
                    },
                },
                'caption': 'nothing special',
                'owners': ['a@b']
            },
        ],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 4)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testValidationSchema(self):
    original = {
        'policy_definitions': [
            {
                'name': 'Policy1',
                'owners': ['a@b'],
                'validation_schema': {
                    'type': 'object',
                    'properties': {
                        'description': 'properties description',
                        'type': 'object',
                    },
                },
            },
        ],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testDescriptionSchema(self):
    original = {
        'policy_definitions': [
            {
                'name': 'Policy1',
                'owners': ['a@b'],
                'description_schema': {
                    'type': 'object',
                    'properties': {
                        'description': 'properties description',
                        'type': 'object',
                    },
                },
            },
        ],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  # Keeping for backwards compatibility.
  def testSubPolicyOldFormat(self):
    original = {
        'policy_definitions': [{
            'type':
            'group',
            'policies': [{
                'name': 'Policy1',
                'caption': 'nothing special',
                'owners': ['a@b']
            }]
        }],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testSubPolicyNewFormat(self):
    original = {
        'policy_definitions': [{
            'type': 'group',
            'policies': ['Policy1']
        }, {
            'name': 'Policy1',
            'caption': 'nothing special',
            'owners': ['a@b']
        }],
        'policy_atomic_group_definitions': [],
        'messages': {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testEscapingAndLineBreaks(self):
    original = {
        'policy_definitions': [],
        'policy_atomic_group_definitions': [],
        'messages': {
            'msg1': {
                # The following line will contain two backslash characters when it
                # ends up in eval().
                'text': '''backslashes, Sir? \\\\''',
                'desc': ''
            },
            'msg2': {
                'text': '''quotes, Madam? "''',
                'desc': ''
            },
            'msg3': {
                # The following line will contain two backslash characters when it
                # ends up in eval().
                'text': 'backslashes, Sir? \\\\',
                'desc': ''
            },
            'msg4': {
                'text': "quotes, Madam? '",
                'desc': ''
            },
            'msg5': {
                'text': '''what happens
with a newline?''',
                'desc': ''
            },
            'msg6': {
                # The following line will contain a backslash+n when it ends up in
                # eval().
                'text': 'what happens\\nwith a newline? (Episode 1)',
                'desc': ''
            }
        }
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 6)
    expected = self.GetExpectedOutput(original)
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))

  def testPlaceholdersChromium(self):
    original = {
        "policy_definitions": [{
            "name": "Policy1",
            "caption":
            "Please install\\n<ph name=\"PRODUCT_NAME\">$1<ex>Google Chrome</ex></ph>.",
            "owners": "a@b"
        }],
        "policy_atomic_group_definitions": [],
        "messages": {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.SetDefines({'_chromium': True})
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = json.loads(re.sub('<ph.*ph>', 'Chromium', json.dumps(original)))
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))
    self.assertTrue(gatherer.GetCliques()[0].translateable)
    msg = gatherer.GetCliques()[0].GetMessage()
    self.assertTrue(len(msg.GetPlaceholders()) == 1)
    ph = msg.GetPlaceholders()[0]
    self.assertTrue(ph.GetOriginal() == 'Chromium')
    self.assertTrue(ph.GetPresentation() == 'PRODUCT_NAME')
    self.assertTrue(ph.GetExample() == 'Google Chrome')

  def testPlaceholdersChrome(self):
    original = {
        "policy_definitions": [{
            "name": "Policy1",
            "caption":
            "Please install\\n<ph name=\"PRODUCT_NAME\">$1<ex>Google Chrome</ex></ph>.",
            "owners": "a@b"
        }],
        "policy_atomic_group_definitions": [],
        "messages": {}
    }
    gatherer = policy_json.PolicyJson(io.StringIO(json.dumps(original)))
    gatherer.SetDefines({'_google_chrome': True})
    gatherer.Parse()
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    expected = json.loads(
        re.sub('<ph.*ph>', 'Google Chrome', json.dumps(original)))
    self.assertTrue(expected == json.loads(gatherer.Translate('en')))
    self.assertTrue(gatherer.GetCliques()[0].translateable)
    msg = gatherer.GetCliques()[0].GetMessage()
    self.assertTrue(len(msg.GetPlaceholders()) == 1)
    ph = msg.GetPlaceholders()[0]
    self.assertTrue(ph.GetOriginal() == 'Google Chrome')
    self.assertTrue(ph.GetPresentation() == 'PRODUCT_NAME')
    self.assertTrue(ph.GetExample() == 'Google Chrome')

  def testGetDescription(self):
    gatherer = policy_json.PolicyJson({})
    gatherer.SetDefines({'_google_chrome': True})
    self.assertEqual(
        gatherer._GetDescription({'name': 'Policy1', 'owners': ['a@b']},
                                 'policy', None, 'desc'),
        'Description of the policy named Policy1 [owner(s): a@b]')
    self.assertEqual(
        gatherer._GetDescription({'name': 'Plcy2', 'owners': ['a@b', 'c@d']},
                                 'policy', None, 'caption'),
        'Caption of the policy named Plcy2 [owner(s): a@b,c@d]')
    self.assertEqual(
        gatherer._GetDescription({'name': 'Plcy3', 'owners': ['a@b']},
                                 'policy', None, 'label'),
        'Label of the policy named Plcy3 [owner(s): a@b]')
    self.assertEqual(
        gatherer._GetDescription({'name': 'Item'}, 'enum_item',
                                 {'name': 'Plcy', 'owners': ['a@b']}, 'caption'),
        'Caption of the option named Item in policy Plcy [owner(s): a@b]')


if __name__ == '__main__':
  unittest.main()
