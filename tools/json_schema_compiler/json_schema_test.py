#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json_schema
import unittest


class JsonSchemaUnittest(unittest.TestCase):

  def testNocompile(self):
    compiled = [{
        "namespace": "compile",
        "description": "The compile API.",
        "functions": [],
        "types": {}
    }, {
        "namespace": "functions",
        "description": "The functions API.",
        "functions": [{
            "id": "two"
        }, {
            "id": "four"
        }],
        "types": {
            "one": {
                "key": "value"
            }
        }
    }, {
        "namespace": "types",
        "description": "The types API.",
        "functions": [{
            "id": "one"
        }],
        "types": {
            "two": {
                "key": "value"
            },
            "four": {
                "key": "value"
            }
        }
    }, {
        "namespace": "nested",
        "description": "The nested API.",
        "properties": {
            "sync": {
                "functions": [{
                    "id": "two"
                }, {
                    "id": "four"
                }],
                "types": {
                    "two": {
                        "key": "value"
                    },
                    "four": {
                        "key": "value"
                    }
                }
            }
        }
    }]

    schema = json_schema.CachedLoad('test/json_schema_test.json')
    self.assertEqual(compiled, json_schema.DeleteNodes(schema, 'nocompile'))

    def should_delete(value):
      return isinstance(value, dict) and not value.get('valid', True)

    expected = [{'one': {'test': 'test'}}, {'valid': True}, {}]
    given = [{
        'one': {
            'test': 'test'
        },
        'two': {
            'valid': False
        }
    }, {
        'valid': True
    }, {}, {
        'valid': False
    }]
    self.assertEqual(expected,
                     json_schema.DeleteNodes(given, matcher=should_delete))


if __name__ == '__main__':
  unittest.main()
