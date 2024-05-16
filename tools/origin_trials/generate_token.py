#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for generating experimental API tokens

usage: generate_token.py [-h] [--key-file KEY_FILE]
                         [--expire-days EXPIRE_DAYS |
                          --expire-timestamp EXPIRE_TIMESTAMP]
                         [--is-subdomain | --no-subdomain]
                         [--is-third-party | --no-third-party]
                         [--usage-restriction USAGE_RESTRICTION]
                         --version=VERSION
                         origin trial_name

Run "generate_token.py -h" for more help on usage.
"""

from __future__ import print_function

import argparse
import base64
import json
import os
import re
import struct
import sys
import time
from datetime import datetime

from six import raise_from
from urllib.parse import urlparse

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'third_party', 'ed25519'))
import ed25519

# Matches a valid DNS name label (alphanumeric plus hyphens, except at the ends,
# no longer than 63 ASCII characters)
DNS_LABEL_REGEX = re.compile(r"^(?!-)[a-z\d-]{1,63}(?<!-)$", re.IGNORECASE)

# Only Version 2 and Version 3 are currently supported.
VERSIONS = {"2": (2, b'\x02'), "3": (3, b'\x03')}

# Only empty string and "subset" are currently supoprted in alternative usage
# resetriction.
USAGE_RESTRICTION = ["", "subset"]

# Default key file, relative to script_dir.
DEFAULT_KEY_FILE = 'eftest.key'


def VersionFromArg(arg):
  """Determines whether a string represents a valid version.
  Only Version 2 and Version 3 are currently supported.

  Returns a tuple of the int and bytes representation of version.
  Returns None if version is not valid.
  """
  return VERSIONS.get(arg, None)


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
  return None


def IsExtensionId(arg):
  """Determines whether a string represents a valid Chromium extension origin.

  Returns True if the argument is valid extension origin, or False otherwise.
  """
  extensionIdRegex = re.compile(r"[a-p]{32}")
  return bool(extensionIdRegex.fullmatch(arg))


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
  origin = urlparse(arg)
  if not origin or not origin.scheme or not origin.netloc:
    raise argparse.ArgumentTypeError("%s is not a hostname or a URL" % arg)
  # HTTPS or HTTP only
  if origin.scheme not in ("https", "http", "chrome-extension"):
    raise argparse.ArgumentTypeError("%s does not use a recognized URL scheme" %
                                     arg)
  # Is it a valid extension origin?
  if origin.scheme == "chrome-extension":
    if (IsExtensionId(origin.hostname) and not origin.port
        and not origin.username and not origin.password):
      return "chrome-extension://{0}".format(origin.hostname)
    raise argparse.ArgumentTypeError("%s is not a valid extension origin" % arg)
  # Add default port if it is not specified
  try:
    port = origin.port
  except ValueError as e:
    raise_from(
        argparse.ArgumentTypeError("%s is not a hostname or a URL" % arg), e)
  if not port:
    port = {"https": 443, "http": 80}[origin.scheme]
  # Strip any extra components and return the origin URL:
  return "{0}://{1}:{2}".format(origin.scheme, origin.hostname, port)

def ExpiryFromArgs(args):
  expiry: int
  if args.expire_timestamp:
    expiry = int(args.expire_timestamp)
  else:
    expiry = (int(time.time()) + (int(args.expire_days) * 86400))

  if expiry > 2**31 - 1:
    # The maximum expiry timestamp is bound by the maximum value of a signed
    # 32-bit integer (2^31-1).
    # TODO(crbug.com/40872096): All expiries after 2038-01-19 03:14:07 UTC
    # will raise this error, so add support for a larger range of values
    # before then.
    raise argparse.ArgumentTypeError(
        "%d (%s UTC) is beyond the range of supported expiries" %
        (expiry, datetime.utcfromtimestamp(expiry)))
  return expiry

def GenerateTokenData(version, origin, is_subdomain, is_third_party,
                      usage_restriction, feature_name, expiry):
  data = {"origin": origin,
          "feature": feature_name,
          "expiry": expiry}
  if is_subdomain is not None:
    data["isSubdomain"] = is_subdomain
  # Only version 3 token supports fields: is_third_party, usage.
  if version == 3 and is_third_party is not None:
    data["isThirdParty"] = is_third_party
  if version == 3 and usage_restriction is not None:
    data["usage"] = usage_restriction
  return json.dumps(data).encode('utf-8')

def GenerateDataToSign(version, data):
  return version + struct.pack(">I",len(data)) + data


def Sign(private_key, data):
  return ed25519.signature(data, private_key[:32], private_key[32:])


def FormatToken(version, signature, data):
  return base64.b64encode(version + signature + struct.pack(">I", len(data)) +
                          data).decode("ascii")


def ParseArgs():
  default_key_file_absolute = os.path.join(script_dir, DEFAULT_KEY_FILE)

  parser = argparse.ArgumentParser(
      description="Generate tokens for enabling experimental features")
  parser.add_argument("--version",
                      help="Token version to use. Currently only version 2 "
                      "and version 3 are supported.",
                      default='3',
                      type=VersionFromArg)
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

  third_party_group = parser.add_mutually_exclusive_group()
  third_party_group.add_argument(
      "--is-third-party",
      help="Token will enable the feature for third "
      "party origins. This option is only available for token version 3",
      dest="is_third_party",
      action="store_true")
  third_party_group.add_argument(
      "--no-third-party",
      help="Token will only match first party origin. This option is only "
      "available for token version 3",
      dest="is_third_party",
      action="store_false")
  parser.set_defaults(is_third_party=None)

  parser.add_argument("--usage-restriction",
                      help="Alternative token usage resctriction. This option "
                      "is only available for token version 3. Currently only "
                      "subset exclusion is supported.")

  expiry_group = parser.add_mutually_exclusive_group()
  expiry_group.add_argument("--expire-days",
                            help="Days from now when the token should expire",
                            type=int,
                            default=42)
  expiry_group.add_argument("--expire-timestamp",
                            help="Exact time (seconds since 1970-01-01 "
                                 "00:00:00 UTC) when the token should expire",
                            type=int)

  return parser.parse_args()


def GenerateTokenAndSignature():
  args = ParseArgs()
  expiry = ExpiryFromArgs(args)

  version_int, version_bytes = args.version

  with open(os.path.expanduser(args.key_file), mode="rb") as key_file:
    private_key = key_file.read(64)

  # Validate that the key file read was a proper Ed25519 key -- running the
  # publickey method on the first half of the key should return the second
  # half.
  if (len(private_key) < 64 or
    ed25519.publickey(private_key[:32]) != private_key[32:]):
    print("Unable to use the specified private key file.")
    sys.exit(1)

  if (not version_int):
    print("Invalid token version. Only version 2 and 3 are supported.")
    sys.exit(1)

  if (args.is_third_party is not None and version_int != 3):
    print("Only version 3 token supports is_third_party flag.")
    sys.exit(1)

  if (args.usage_restriction is not None):
    if (version_int != 3):
      print("Only version 3 token supports alternative usage restriction.")
      sys.exit(1)
    if (args.usage_restriction not in USAGE_RESTRICTION):
      print(
          "Only empty string and \"subset\" are supported in alternative usage "
          "restriction.")
      sys.exit(1)
  token_data = GenerateTokenData(version_int, args.origin, args.is_subdomain,
                                 args.is_third_party, args.usage_restriction,
                                 args.trial_name, expiry)
  data_to_sign = GenerateDataToSign(version_bytes, token_data)
  signature = Sign(private_key, data_to_sign)

  # Verify that that the signature is correct before printing it.
  try:
    ed25519.checkvalid(signature, data_to_sign, private_key[32:])
  except Exception as exc:
    print("There was an error generating the signature.")
    print("(The original error was: %s)" % exc)
    sys.exit(1)

  token_data = GenerateTokenData(version_int, args.origin, args.is_subdomain,
                                 args.is_third_party, args.usage_restriction,
                                 args.trial_name, expiry)
  data_to_sign = GenerateDataToSign(version_bytes, token_data)
  signature = Sign(private_key, data_to_sign)
  return args, token_data, signature, expiry


def main():
  args, token_data, signature, expiry = GenerateTokenAndSignature()
  version_int, version_bytes = args.version

  # Output the token details
  print("Token details:")
  print(" Version: %s" % version_int)
  print(" Origin: %s" % args.origin)
  print(" Is Subdomain: %s" % args.is_subdomain)
  if version_int == 3:
    print(" Is Third Party: %s" % args.is_third_party)
    print(" Usage Restriction: %s" % args.usage_restriction)
  print(" Feature: %s" % args.trial_name)
  print(" Expiry: %d (%s UTC)" % (expiry, datetime.utcfromtimestamp(expiry)))
  print(" Signature: %s" % ", ".join('0x%02x' % x for x in signature))
  b64_signature = base64.b64encode(signature).decode("ascii")
  print(" Signature (Base64): %s" % b64_signature)
  print()

  # Output the properly-formatted token.
  print(FormatToken(version_bytes, signature, token_data))


if __name__ == "__main__":
  main()
