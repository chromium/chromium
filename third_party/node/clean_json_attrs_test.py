#!/usr/bin/env python

import clean_json_attrs
import json
import os
import shutil
import tempfile
import unittest

class CleanJsonAttrs(unittest.TestCase):
  def setUp(self):
    self._start_dir = tempfile.mkdtemp(dir=os.path.dirname(__file__))
    self._kwargs = {
        'file_pattern': 'package\.json',
        'attr_pattern': '^_',
        'start_dir': self._start_dir
    }

  def tearDown(self):
    assert self._start_dir
    shutil.rmtree(self._start_dir)

  def _read_temp_file(self, filename):
    return json.loads(open(os.path.join(self._start_dir, filename)).read())

  def _write_temp_file(self, filename, json_dict):
    with open(os.path.join(self._start_dir, filename), 'w') as f:
      f.write(json.dumps(json_dict))

  def testAttrPattern(self):
    self._write_temp_file('package.json', {
        'delete_me': True,
        'ignore_me': True,
        'version': '2.3.4',
    })
    args = self._kwargs.copy()
    args['attr_pattern'] = '^delete'
    self.assertTrue(clean_json_attrs.Clean(**args))
    json_dict = self._read_temp_file('package.json')
    self.assertEquals(['ignore_me', 'version'], sorted(json_dict.keys()))

  def testFilePattern(self):
    self._write_temp_file('clean_me.json', {'_where': '/a/b/c'})
    self._write_temp_file('ignore_me.json', {'_args': ['/a/b/c']})
    args = self._kwargs.copy()
    args['file_pattern'] = '^clean_'
    self.assertTrue(clean_json_attrs.Clean(**args))
    self.assertEquals([], self._read_temp_file('clean_me.json').keys())
    self.assertEquals(['_args'], self._read_temp_file('ignore_me.json').keys())

  def testNestedKeys(self):
    self._write_temp_file('package.json', {
        '_args': ['/some/path/'],
        'nested': {
            '_keys': [],
            'also': {
                '_get': 'scanned',
            },
        },
        '_where': '/some/path',
        'version': '2.0.0'
    })
    self.assertTrue(clean_json_attrs.Clean(**self._kwargs))
    json_dict = self._read_temp_file('package.json')
    self.assertEquals(['nested', 'version'], sorted(json_dict.keys()))
    self.assertEquals(['also'], json_dict['nested'].keys())
    self.assertEquals([], json_dict['nested']['also'].keys())

  def testNothingToRemove(self):
    self._write_temp_file('package.json', {'version': '2.0.0'})
    self.assertFalse(clean_json_attrs.Clean(**self._kwargs))
    self.assertEquals(['version'], self._read_temp_file('package.json').keys())

  def testSimple(self):
    self._write_temp_file('package.json', {
        '_args': ['/some/path/'],
        'version': '2.0.0',
        '_where': '/some/path'
    })
    self.assertTrue(clean_json_attrs.Clean(**self._kwargs))
    self.assertEquals(['version'], self._read_temp_file('package.json').keys())


if __name__ == '__main__':
  unittest.main()
