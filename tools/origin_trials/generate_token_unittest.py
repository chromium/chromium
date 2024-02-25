#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_token.py"""

import argparse
import generate_token
from unittest import main, mock, TestCase


class GenerateTokenTest(TestCase):

  def test_hostname_validation(self):
    for hostname, expected_result in [
        ("", None),
        (None, None),
        ("example.com", "example.com"),
        ("127.0.0.1", "127.0.0.1"),
        ("localhost", "localhost"),
        ("example.com.", "example.com"),
        ("ExAmPlE.coM", "example.com"),
        (".example.com", None),
        ("example..com", None),
        ("example123.com", "example123.com"),
        ("123example.com", "123example.com"),
        ("a.com", "a.com"),
        ("1.com", "1.com"),
        ("-.com", None),
        ("aa.com", "aa.com"),
        ("a1.com", "a1.com"),
        ("a-.com", None),
        ("-a.com", None),
        ("123-example.com", "123-example.com"),
        ("-123example.com", None),
        ("123example-.com", None),
        (("a"*63)+".com", ("a"*63)+".com"),
        (("a"*64)+".com", None),
        (".".join([("a"*15)]*16), ".".join([("a"*15)]*16)),
        (".".join([("a"*15)]*17), None)]:
      self.assertEqual(generate_token.HostnameFromArg(hostname),
                       expected_result)

  def test_extension_id_validation(self):
    for extension_id, expected_result in [("a" * 32, True), ("p" * 32, True),
                                          ("a" * 2, False), ("a" * 33, False),
                                          ("q" * 32, False), ("A" * 32, False)]:
      self.assertEqual(generate_token.IsExtensionId(extension_id),
                       expected_result)

  def test_origin_constructed_correctly(self):
    for origin_arg, expected_result in [
        ("example.com", "https://example.com:443"),
        ("https://example.com", "https://example.com:443"),
        ("https://example.com/", "https://example.com:443"),
        ("http://example.com", "http://example.com:80"),
        ("http://127.0.0.1:8000", "http://127.0.0.1:8000"),
        ("http://user:pass@example.com/path", "http://example.com:80"),
        ("chrome-extension://" + "a" * 32, "chrome-extension://" + "a" * 32),
        ("chrome-extension://" + "a" * 32 + "/",
         "chrome-extension://" + "a" * 32)
    ]:
      self.assertEqual(generate_token.OriginFromArg(origin_arg),
                       expected_result)

  def test_origin_fails_correctly(self):
    for invalid_hostname in [
        "example..com", "gopher://gopher.tc.umn.edu", "https://",
        "https://example.com:NaN/", "chrome-extension://user:pass@" + "a" * 32,
        "chrome-extension://" + "a" * 32 + ":1", "chrome-extension://aaa",
        "chrome-extension://" + "a" * 32 + "x",
        "chrome-extension://x" + "a" * 32, "Not even close"
    ]:
      self.assertRaises(argparse.ArgumentTypeError,
                        generate_token.OriginFromArg,
                        invalid_hostname)

  def test_end_to_end(self):
    with mock.patch('sys.argv',
                    ['generate-token.py', 'example.com', 'example']):
      generate_token.GenerateTokenAndSignature()

  def test_FormatToken(self):
    for version, signature, token_data, expected in [
        (b'\x03', bytes([1, 2, 3]), bytes([4, 5, 6]), 'AwECAwAAAAMEBQY='),
        (b'\x03', bytes([200, 100, 1]), bytes([30, 40,
                                               50]), 'A8hkAQAAAAMeKDI='),
        (b'\x02', bytes([2, 3, 2]), bytes([2, 3, 2]), 'AgIDAgAAAAMCAwI='),
        (b'\x02', bytes([255, 150, 10]), bytes([10, 150,
                                                255]), 'Av+WCgAAAAMKlv8=')
    ]:
      self.assertEqual(
          generate_token.FormatToken(version, signature, token_data), expected)


if __name__ == '__main__':
  main()
