#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for generating experimental API tokens

usage: generate_token.py [-h] [--key-file KEY_FILE]
                         [--expire-days EXPIRE_DAYS |
                          --expire-timestamp EXPIRE_TIMESTAMP]
                         [--is_subdomain | --no-subdomain]
                         origin trial_name

Run "generate_token.py -h" for more help on usage.
"""

from __future__ import print_function

import argparse
import base64
from datetime import datetime
import json
import re
import os
import struct
import sys
import time
import urlparse

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'third_party', 'ed25519'))
import ed25519


# Matches a valid DNS name label (alphanumeric plus hyphens, except at the ends,
# no longer than 63 ASCII characters)
DNS_LABEL_REGEX = re.compile(r"^(?!-)[a-z\d-]{1,63}(?<!-)$", re.IGNORECASE)

# This script generates Version 2 tokens.
VERSION = "\x02"

# Default key file, relative to script_dir.
DEFAULT_KEY_FILE = 'eftest.key'

def HostnameFromArg(arg):
  """Determines whether a string represents a valid hostname.

  Returns the canonical hostname if its argument is valid, or None otherwise.
  """
  if not arg or len(arg) > 255:
    return None
  if arg[-1] == ".":
    arg = arg[:-1]
  if "." not in arg and arg != "localhost":
    return None
  if all(DNS_LABEL_REGEX.match(label) for label in arg.split(".")):
    return arg.lower()

def OriginFromArg(arg):
  """Constructs the origin for the token from a command line argument.

  Returns None if this is not possible (neither a valid hostname nor a
  valid origin URL was provided.)
  """
  # Does it look like a hostname?
  hostname = HostnameFromArg(arg)
  if hostname:
    return "https://" + hostname + ":443"
  # If not, try to construct an origin URL from the argument
  origin = urlparse.urlparse(arg)
  if not origin or not origin.scheme or not origin.netloc:
    raise argparse.ArgumentTypeError("%s is not a hostname or a URL" % arg)
  # HTTPS or HTTP only
  if origin.scheme not in ('https','http'):
    raise argparse.ArgumentTypeError("%s does not use a recognized URL scheme" %
                                     arg)
  # Add default port if it is not specified
  try:
    port = origin.port
  except ValueError:
    raise argparse.ArgumentTypeError("%s is not a hostname or a URL" % arg)
  if not port:
    port = {"https": 443, "http": 80}[origin.scheme]
  # Strip any extra components and return the origin URL:
  return "{0}://{1}:{2}".format(origin.scheme, origin.hostname, port)

def ExpiryFromArgs(args):
  if args.expire_timestamp:
    return int(args.expire_timestamp)
  return (int(time.time()) + (int(args.expire_days) * 86400))

def GenerateTokenData(origin, is_subdomain, feature_name, expiry):
  data = {"origin": origin,
          "feature": feature_name,
          "expiry": expiry}
  if is_subdomain is not None:
    data["isSubdomain"] = is_subdomain
  return json.dumps(data).encode('utf-8')

def GenerateDataToSign(version, data):
  return version + struct.pack(">I",len(data)) + data

def Sign(private_key, data):
  return ed25519.signature(data, private_key[:32], private_key[32:])

def FormatToken(version, signature, data):
  return base64.b64encode(version + signature +
                          struct.pack(">I",len(data)) + data)

def main():
  default_key_file_absolute = os.path.join(script_dir, DEFAULT_KEY_FILE)

  parser = argparse.ArgumentParser(
      description="Generate tokens for enabling experimental features")
  parser.add_argument("origin",
                      help="Origin for which to enable the feature. This can "
                           "be either a hostname (default scheme HTTPS, "
                           "default port 443) or a URL.",
                      type=OriginFromArg)
  parser.add_argument("trial_name",
                      help="Feature to enable. The current list of "
                           "experimental feature trials can be found in "
                           "RuntimeFeatures.in")
  parser.add_argument("--key-file",
                      help="Ed25519 private key file to sign the token with",
                      default=default_key_file_absolute)

  subdomain_group = parser.add_mutually_exclusive_group()
  subdomain_group.add_argument("--is-subdomain",
                               help="Token will enable the feature for all "
                                    "subdomains that match the origin",
                               dest="is_subdomain",
                               action="store_true")
  subdomain_group.add_argument("--no-subdomain",
                               help="Token will only match the specified "
                                    "origin (default behavior)",
                               dest="is_subdomain",
                               action="store_false")
  parser.set_defaults(is_subdomain=None)

  expiry_group = parser.add_mutually_exclusive_group()
  expiry_group.add_argument("--expire-days",
                            help="Days from now when the token should expire",
                            type=int,
                            default=42)
  expiry_group.add_argument("--expire-timestamp",
                            help="Exact time (seconds since 1970-01-01 "
                                 "00:00:00 UTC) when the token should expire",
                            type=int)

  args = parser.parse_args()
  expiry = ExpiryFromArgs(args)

  key_file = open(os.path.expanduser(args.key_file), mode="rb")
  private_key = key_file.read(64)

  # Validate that the key file read was a proper Ed25519 key -- running the
  # publickey method on the first half of the key should return the second
  # half.
  if (len(private_key) < 64 or
    ed25519.publickey(private_key[:32]) != private_key[32:]):
    print("Unable to use the specified private key file.")
    sys.exit(1)

  token_data = GenerateTokenData(args.origin, args.is_subdomain,
                                 args.trial_name, expiry)
  data_to_sign = GenerateDataToSign(VERSION, token_data)
  signature = Sign(private_key, data_to_sign)

  # Verify that that the signature is correct before printing it.
  try:
    ed25519.checkvalid(signature, data_to_sign, private_key[32:])
  except Exception, exc:
    print("There was an error generating the signature.")
    print("(The original error was: %s)" % exc)
    sys.exit(1)


  # Output the token details
  print("Token details:")
  print(" Origin: %s" % args.origin)
  print(" Is Subdomain: %s" % args.is_subdomain)
  print(" Feature: %s" % args.trial_name)
  print(" Expiry: %d (%s UTC)" % (expiry, datetime.utcfromtimestamp(expiry)))
  print(" Signature: %s" % ", ".join('0x%02x' % ord(x) for x in signature))
  print(" Signature (Base64): %s" % base64.b64encode(signature))
  print()

  # Output the properly-formatted token.
  print(FormatToken(VERSION, signature, token_data))


if __name__ == "__main__":
  main()
