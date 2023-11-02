#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for validating and inspecting origin trial tokens

usage: check_token.py [-h] [--use-chrome-key |
                            --use-test-key |
                            --private-key-file KEY_FILE]
                           "base64-encoded token"

Run "check_token.py -h" for more help on usage.
"""

from __future__ import print_function

import argparse
import base64
from datetime import datetime
import json
import os
import struct
import sys
import time

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'third_party', 'ed25519'))
import ed25519

# Version is a 1-byte field at offset 0.
#  - To support version-dependent formats, the version number must be the first
#    first part of the token.
VERSION_OFFSET = 0
VERSION_SIZE = 1

# These constants define the Version 2 field sizes and offsets.
# Contents are: version|signature|payload length|payload
SIGNATURE_OFFSET = VERSION_OFFSET + VERSION_SIZE
SIGNATURE_SIZE = 64
PAYLOAD_LENGTH_OFFSET = SIGNATURE_OFFSET + SIGNATURE_SIZE
PAYLOAD_LENGTH_SIZE = 4
PAYLOAD_OFFSET = PAYLOAD_LENGTH_OFFSET + PAYLOAD_LENGTH_SIZE

# This script supports Version 2 and Version 3 tokens.
VERSION2 = b'\x02'
VERSION3 = b'\x03'

# Only empty string and "subset" are supported in alternative usage restriction.
USAGE_RESTRICTION = ["", "subset"]

# Chrome public key, used by default to validate signatures
#  - Copied from chrome/common/origin_trials/chrome_origin_trial_policy.cc
CHROME_PUBLIC_KEY = bytes([
    0x7c,
    0xc4,
    0xb8,
    0x9a,
    0x93,
    0xba,
    0x6e,
    0xe2,
    0xd0,
    0xfd,
    0x03,
    0x1d,
    0xfb,
    0x32,
    0x66,
    0xc7,
    0x3b,
    0x72,
    0xfd,
    0x54,
    0x3a,
    0x07,
    0x51,
    0x14,
    0x66,
    0xaa,
    0x02,
    0x53,
    0x4e,
    0x33,
    0xa1,
    0x15,
])

# Default key file, relative to script_dir.
DEFAULT_KEY_FILE = 'eftest.key'


class OverrideKeyFileAction(argparse.Action):
  def __call__(self, parser, namespace, values, option_string=None):
    setattr(namespace, "use_chrome_key", None)
    setattr(namespace, self.dest, values)


def main():
  parser = argparse.ArgumentParser(
      description="Inspect origin trial tokens")
  parser.add_argument("token",
                      help="Token to be checked (must be Base64 encoded)")

  key_group = parser.add_mutually_exclusive_group()
  key_group.add_argument("--use-chrome-key",
                         help="Validate token using the real Chrome public key",
                         dest="use_chrome_key",
                         action="store_true")
  key_group.add_argument("--use-test-key",
                         help="Validate token using the eftest.key",
                         dest="use_chrome_key",
                         action="store_false")
  key_group.add_argument("--key-file",
                         help="Ed25519 private key file to validate the token",
                         dest="key_file",
                         action=OverrideKeyFileAction)
  parser.set_defaults(use_chrome_key=False)

  args = parser.parse_args()

  # Figure out which public key to use: Chrome, test key (default option), or
  # key file provided on command line.
  public_key = None
  private_key_file = None
  if (args.use_chrome_key is None):
    private_key_file = args.key_file
  else:
    if (args.use_chrome_key):
      public_key = CHROME_PUBLIC_KEY
    else:
      # Use the test key, relative to this script.
      private_key_file = os.path.join(script_dir, DEFAULT_KEY_FILE)

  # If not using the Chrome public key, extract the public key from either the
  # test key file, or the private key file provided on the command line.
  if public_key is None:
    try:
      key_file = open(os.path.expanduser(private_key_file), mode="rb")
    except IOError as exc:
      print("Unable to open key file: %s" % private_key_file)
      print("(%s)" % exc)
      sys.exit(1)

    private_key = key_file.read(64)

    # Validate that the key file read was a proper Ed25519 key -- running the
    # publickey method on the first half of the key should return the second
    # half.
    if (len(private_key) < 64 or
      ed25519.publickey(private_key[:32]) != private_key[32:]):
      print("Unable to use the specified private key file.")
      sys.exit(1)

    public_key = private_key[32:]

  try:
    token_contents = base64.b64decode(args.token)
  except TypeError as exc:
    print("Error decoding the token (%s)" % exc)
    sys.exit(1)


  # Only version 2 and version 3 currently supported.
  if (len(token_contents) < (VERSION_OFFSET + VERSION_SIZE)):
    print("Token is malformed - too short.")
    sys.exit(1)

  version = token_contents[VERSION_OFFSET:(VERSION_OFFSET + VERSION_SIZE)]
  # Convert the version string to a number
  version_number = 0
  for x in version:
    version_number <<= 8
    version_number += x
  if (version not in (VERSION2, VERSION3)):
    print("Token has wrong version: %d" % version_number)
    sys.exit(1)

  # Token must be large enough to contain a version, signature, and payload
  # length.
  minimum_token_length = PAYLOAD_LENGTH_OFFSET + PAYLOAD_LENGTH_SIZE
  if (len(token_contents) < minimum_token_length):
    print("Token is malformed - too short: %d bytes, minimum is %d" % \
      (len(token_contents), minimum_token_length))
    sys.exit(1)

  # Extract the length of the signed data (Big-endian).
  # (unpack returns a tuple).
  payload_length = struct.unpack_from(">I", token_contents,
                                      PAYLOAD_LENGTH_OFFSET)[0]

  # Validate that the stated length matches the actual payload length.
  actual_payload_length = len(token_contents) - PAYLOAD_OFFSET
  if (payload_length != actual_payload_length):
    print("Token is %d bytes, expected %d" % (actual_payload_length,
                                              payload_length))
    sys.exit(1)

  # Extract the version-specific contents of the token.
  # Contents are: version|signature|payload length|payload
  signature = token_contents[SIGNATURE_OFFSET:PAYLOAD_LENGTH_OFFSET]

  # The data which is covered by the signature is (version + length + payload).
  signed_data = version + token_contents[PAYLOAD_LENGTH_OFFSET:]

  # Validate the signature on the data.
  try:
    ed25519.checkvalid(signature, signed_data, public_key)
  except Exception as exc:
    print("Signature invalid (%s)" % exc)
    sys.exit(1)

  try:
    payload = token_contents[PAYLOAD_OFFSET:].decode('utf-8')
  except UnicodeError as exc:
    print("Unable to decode token contents (%s)" % exc)
    sys.exit(1)

  try:
    token_data = json.loads(payload)
  except Exception as exc:
    print("Unable to parse payload (%s)" % exc)
    print("Payload: %s" % payload)
    sys.exit(1)

  print()
  print("Token data: %s" % token_data)
  print()

  # Extract the required fields
  for field in ["origin", "feature", "expiry"]:
    if field not in token_data:
      print("Token is missing required field: %s" % field)
      sys.exit(1)

  origin = token_data["origin"]
  trial_name = token_data["feature"]
  expiry = token_data["expiry"]

  # Extract the optional fields
  is_subdomain = token_data.get("isSubdomain")
  is_third_party = token_data.get("isThirdParty")
  if (is_third_party is not None and version != VERSION3):
    print("The isThirdParty field can only be be set in Version 3 token.")
    sys.exit(1)

  usage_restriction = token_data.get("usage")
  if (usage_restriction is not None and version != VERSION3):
    print("The usage field can only be be set in Version 3 token.")
    sys.exit(1)
  if (usage_restriction is not None
      and usage_restriction not in USAGE_RESTRICTION):
    print("Only empty string and \"subset\" are supported in the usage field.")
    sys.exit(1)

  # Output the token details
  print("Token details:")
  print(" Version: %s" % version_number)
  print(" Origin: %s" % origin)
  print(" Is Subdomain: %s" % is_subdomain)
  if (version == VERSION3):
    print(" Is Third Party: %s" % is_third_party)
    print(" Usage Restriction: %s" % usage_restriction)
  print(" Feature: %s" % trial_name)
  print(" Expiry: %d (%s UTC)" % (expiry, datetime.utcfromtimestamp(expiry)))
  print(" Signature: %s" % ", ".join('0x%02x' % x for x in signature))
  b64_signature = base64.b64encode(signature).decode("ascii")
  print(" Signature (Base64): %s" % b64_signature)
  print()

if __name__ == "__main__":
  main()
