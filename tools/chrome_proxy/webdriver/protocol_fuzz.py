# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This test checks that Chrome does not crash or otherwise wildly misbehave when
# it receives unexpected Chrome-Proxy header values and/or directives. This is
# done by fuzz testing which is configured through a dictionary object at the
# top of this file.
#
# The fuzz testing generates URLs which are requested from the Chrome Proxy test
# server which will modify its response based on the encoded data in the URL.
#
# The fuzz testing is configured in a dictionary-type object of the following
# format:
#   {
#     # This specifies a header that will be fuzzed. Only one header is fuzzed
#     # per test. When a header is being fuzz tested, any entry it has in the
#     # STATIC_RESPONSE_HEADERS object will be overwritten.
#     "chrome-proxy": {
#       # This specifies the directive key values that are possible to use in
#       # fuzzing.
#       "directive_keys: [
#         "page-policy",
#         "foo",
#       ],
#       # This specifies the directive values that are possible to use in
#       # fuzzing. Any one of these values may end up mapped to any one of the
#       # directive_keys above.
#       "directive_values": [
#         "empty-image",
#         "bar",
#       ],
#       # The maximum number of directives to use in this header. If this value
#       # is greater than the number of directives given above, that will be
#       # used instead.
#       "max_directives": 10,
#
#       # The maximum number of directive values to use per directive. If this
#       # value is greater than the number of directive values given above, that
#       # will be used instead.
#       "max_directive_values": 10,
#
#       # This is used to join the directive values when multiple are used.
#       "directive_value_joiner": "|",
#     },
#   }
#
# If the above configuration was given, the following values would be generated
# for the chrome-proxy header:
#   <empty-string>
#   foo
#   foo=bar
#   foo=empty-image
#   foo=bar|empty-image
#   page_policies
#   page_policies=bar
#   page_policies=empty-image
#   page_policies=bar|empty-image
#   foo,page_policies
#   foo=bar,page_policies=bar
#   foo=empty-image,page_policies=empty-image
#   foo=bar|empty-image,page_policies=bar|empty-image
#
# Randomly generated values are also supported in the fuzz_header field
# ("chrome-proxy" in the above example), directive_keys, and directive_values.
# Randomly generated values can be specified using these formats:
#   {{RAND_STR(N)}}
#     Creates a random string of lowercase and digit characters of length N
#
#   {{RAND_INT(N)}}
#     Creates a random integer in the range [0, 10^N+1) with leading zeros
#     Example: "Num cookies: {{RAND_INT(4)}}" yields "Num cookies: 0123"
#
#   {{RAND_DBL(P.Q)}}
#     Creates a random double in the range [0, 10^P+1) with leading zeros and
#     up to Q places after the decimal point
#     Example: "My money = ${{RAND_DBL(3,2)}}" yields "My money = $001.53"
#
#   {{RAND_BOOL}}
#     Creates a random boolean, either 'true' or 'false'
#     Example: "I am awesome: {{RAND_BOOL}}" yields "I am awesome: true"

from __future__ import print_function

import BaseHTTPServer
import base64
import itertools
import json
import random
import re
import string

from common import TestDriver
from common import IntegrationTest
from decorators import Slow

# This dict configures how the fuzzing will operate. See documentation above for
# more information.
FUZZ_HEADERS = {
  "chrome-proxy": {
    "directive_keys": [
      "{{RAND_STR(10)}}",
      "{{RAND_STR(10)}}",
      "page-policies",
    ],
    "directive_values": [
      "empty-image",
      "{{RAND_INT(1)}}",
      "{{RAND_INT(4)}}",
      "{{RAND_STR(10)}}",
      "{{RAND_STR(10)}}",
      "{{RAND_STR(10)}}",
    ],
    "max_directives": 3,
    "max_directive_values": 3,
    "directive_value_joiner": "|",
  },
}

TEST_SERVER = "chromeproxy-test.appspot.com"

# These headers will be present in every test server response. If one of these
# entries is also a fuzzed header above, then the fuzzed value will take the
# place of the static one instead.
STATIC_RESPONSE_HEADERS = {
  "content-type":  ["text/html"],
  "via":           ["1.1 Chrome-Compression-Proxy"],
  "cache-control": ["no-cache, no-store, must-revalidate"],
  "pragma":        ["no-cache"],
  "expires":       ["0"],
}

# This string will be used as the response body in every test and will be
# checked for existence on the final loaded page.
STATIC_RESPONSE_BODY = 'ok'

rand_str_re = re.compile(r'{{RAND_STR\((\d+)\)}}')
rand_int_re = re.compile(r'{{RAND_INT\((\d+)\)}}')
rand_dbl_re = re.compile(r'{{RAND_DBL\((\d+)\.(\d+)\)}}')
rand_bool_re = re.compile(r'{{RAND_BOOL}}')

def ParseRand(key, val):
  """This helper function parses the {{RAND}} expressions in the given values
  and returns them with random values subsituted in place.

  Args:
    key: the header key with 0 or more {{RAND}} expressions
    val: the header value with 0 or more {{RAND}} expressions
  Returns:
    A key, value tuple with subsituted random values
  """
  def GenerateRand(length, charset):
    return ''.join(random.choice(charset) for _ in range(length))
  def _parse_rand(v):
    result = v
    had_match = True
    while had_match:
      had_match = False
      str_match = rand_str_re.search(result)
      if str_match:
        had_match = True
        mag = int(result[str_match.start(1):str_match.end(1)])
        rand_str = GenerateRand(mag, string.ascii_lowercase + string.digits)
        result = (result[:str_match.start()] + rand_str
          + result[str_match.end():])
      int_match = rand_int_re.search(result)
      if int_match:
        had_match = True
        mag = int(result[int_match.start(1):int_match.end(1)])
        rand_int = GenerateRand(mag, string.digits)
        result = (result[:int_match.start()] + rand_int
          + result[int_match.end():])
      dbl_match = rand_dbl_re.search(result)
      if dbl_match:
        had_match = True
        magN = int(result[dbl_match.start(1):dbl_match.end(1)])
        magD = int(result[dbl_match.start(2):dbl_match.end(2)])
        rand_dbl = GenerateRand(magN, string.digits) + '.' + GenerateRand(magD,
          string.digits)
        result = (result[:dbl_match.start()] + rand_dbl
          + result[dbl_match.end():])
      bool_match = rand_bool_re.search(result)
      if bool_match:
        had_match = True
        rand_bool = bool(random.getrandbits(1))
        result = (result[:bool_match.start()] + str(rand_bool).lower()
          + result[bool_match.end():])
    return result
  return (_parse_rand(key), _parse_rand(val))

def GenerateFuzzedHeaders(cfg=FUZZ_HEADERS):
  """This function yields header key value pairs which can be used to update a
  Python dict representing HTTP headers. See file level documentation for more
  information.

  Args:
    cfg: the configuration dict that specifies how to fuzz the proxy headers
  Yields:
    one header key value pair
  """
  for header_key in cfg:
    fuzz = cfg[header_key]
    dirs = fuzz['directive_keys']
    vals = fuzz['directive_values']
    max_dirs = min(fuzz['max_directives'], len(dirs))
    max_vals = min(fuzz['max_directive_values'], len(vals))
    def GenerateFuzzedValues():
      for n in range(0, max_vals + 1):
        for c in itertools.combinations(vals, n):
          yield c
    # Yield an empty header key,value pair before doing all the combinations.
    yield (header_key, '')
    for num_dirs in range(1, max_dirs + 1):
      for directive_set in itertools.combinations(dirs, num_dirs):
        for values in GenerateFuzzedValues():
          value_list = list(values)
          if '' in value_list:
            value_list.remove('')
          value_str = fuzz['directive_value_joiner'].join(value_list)
          header = []
          for directive in directive_set:
            if len(value_str) == 0:
              header.append(directive)
            else:
              header.append('%s=%s' % (directive, value_str))
          yield ParseRand(header_key, ','.join(header))

class FuzzUnitTests(IntegrationTest):

  def testParseRand(self):
    tests = {
      "{{RAND_STR(1)}": r"{{RAND_STR\(1\)}",
      "{{RAND_INT(1)}": r"{{RAND_INT\(1\)}",
      "{{RAND_DBL(1.1)}": r"{{RAND_DBL\(1\.1\)}",
      "{{RAND_DBL(11)}}": r"{{RAND_DBL\(11\)}}",
      "{{RAND_BOOL}": r"{{RAND_BOOL}",
      "{{RAND_STR(0)}}": "",
      "hi{{RAND_STR(0)}}": "hi",
      "{{RAND_STR(0)}}there": "there",
      "hi{{RAND_STR(0)}}there": "hithere",
      "{{RAND_STR(3)}}": r"[a-z0-9][a-z0-9][a-z0-9]",
      "{{RAND_STR(3)}}there": r"[a-z0-9][a-z0-9][a-z0-9]there",
      "hi{{RAND_STR(3)}}": r"hi[a-z0-9][a-z0-9][a-z0-9]",
      "hi{{RAND_STR(3)}}there": r"hi[a-z0-9][a-z0-9][a-z0-9]there",
      "{{RAND_INT(0)}}": "",
      "hi{{RAND_INT(0)}}": "hi",
      "{{RAND_INT(0)}}there": "there",
      "hi{{RAND_INT(0)}}there": "hithere",
      "{{RAND_INT(3)}}": r"[0-9][0-9][0-9]",
      "{{RAND_INT(3)}}there": r"[0-9][0-9][0-9]there",
      "hi{{RAND_INT(3)}}": r"hi[0-9][0-9][0-9]",
      "hi{{RAND_INT(3)}}there": r"hi[0-9][0-9][0-9]there",
      "{{RAND_DBL(0.0)}}": r"\.",
      "hi{{RAND_DBL(0.0)}}": r"hi\.",
      "{{RAND_DBL(0.0)}}there": r"\.there",
      "hi{{RAND_DBL(0.0)}}there": r"hi\.there",
      "{{RAND_DBL(3.3)}}": r"[0-9][0-9][0-9]\.[0-9][0-9][0-9]",
      "hi{{RAND_DBL(3.3)}}": r"hi[0-9][0-9][0-9]\.[0-9][0-9][0-9]",
      "{{RAND_DBL(3.3)}}there": r"[0-9][0-9][0-9]\.[0-9][0-9][0-9]there",
      "hi{{RAND_DBL(3.3)}}there": r"hi[0-9][0-9][0-9]\.[0-9][0-9][0-9]there",
      "{{RAND_BOOL}}": r"(true|false)",
      "{{RAND_BOOL}}there": r"(true|false)there",
      "hi{{RAND_BOOL}}": r"hi(true|false)",
      "hi{{RAND_BOOL}}there": r"hi(true|false)there",
      "{{RAND_STR(1)}}{{RAND_STR(1)}}": r"[a-z0-9][a-z0-9]",
      "{{RAND_STR(1)}}{{RAND_INT(1)}}": r"[a-z0-9][0-9]",
      "{{RAND_STR(1)}}{{RAND_DBL(1.1)}}": r"[a-z0-9][0-9]\.[0-9]",
      "{{RAND_STR(1)}}{{RAND_BOOL}}": r"[a-z0-9](true|false)",
      "{{RAND_INT(1)}}{{RAND_STR(1)}}": r"[0-9][a-z0-9]",
      "{{RAND_INT(1)}}{{RAND_INT(1)}}": r"[0-9][0-9]",
      "{{RAND_INT(1)}}{{RAND_DBL(1.1)}}": r"[0-9][0-9]\.[0-9]",
      "{{RAND_INT(1)}}{{RAND_BOOL}}": r"[0-9](true|false)",
      "{{RAND_DBL(1.1)}}{{RAND_STR(1)}}": r"[0-9]\.[0-9][a-z0-9]",
      "{{RAND_DBL(1.1)}}{{RAND_INT(1)}}": r"[0-9]\.[0-9][0-9]",
      "{{RAND_DBL(1.1)}}{{RAND_DBL(1.1)}}": r"[0-9]\.[0-9][0-9]\.[0-9]",
      "{{RAND_DBL(1.1)}}{{RAND_BOOL}}": r"[0-9]\.[0-9](true|false)",
      "{{RAND_BOOL}}{{RAND_STR(1)}}": r"(true|false)[a-z0-9]",
      "{{RAND_BOOL}}{{RAND_INT(1)}}": r"(true|false)[0-9]",
      "{{RAND_BOOL}}{{RAND_DBL(1.1)}}": r"(true|false)[0-9]\.[0-9]",
      "{{RAND_BOOL}}{{RAND_BOOL}}": r"(true|false)(true|false)",
    }
    for t in tests:
      expected = re.compile('^' + tests[t] + '$')
      gotK, gotV = ParseRand(t, t)
      if not expected.match(gotK):
        self.fail("%s doesn't match /%s/" % (gotK, tests[t]))
      if not expected.match(gotV):
        self.fail("%s doesn't match /%s/" % (gotK, tests[t]))

  def testGenerator(self):
    test_cfg = {
      "chrome-proxy": {
        "directive_keys": [
          "foo",
          "page_policies",
        ],
        "directive_values": [
          "bar",
          "empty-image",
        ],
        "max_directives": 10,
        "max_directive_values": 10,
        "directive_value_joiner": "|",
      },
    }
    expected_headers = ['', 'foo', 'foo=bar', 'foo=empty-image',
      'foo=bar|empty-image', 'page_policies', 'page_policies=bar',
      'page_policies=empty-image', 'page_policies=bar|empty-image',
      'foo,page_policies', 'foo=bar,page_policies=bar',
      'foo=empty-image,page_policies=empty-image',
      'foo=bar|empty-image,page_policies=bar|empty-image',
    ]
    actual_headers = []
    for h in GenerateFuzzedHeaders(cfg=test_cfg):
      actual_headers.append(h[1])
    expected_headers.sort()
    actual_headers.sort()
    self.assertEqual(expected_headers, actual_headers)

class ProtocolFuzzer(IntegrationTest):

  def GenerateTestURLs(self):
    """This function yields test URLs which will cause the test server to
    respond with the given given headers and body.

    Yields:
      URLs suitable for testing fuzzed response headers
    """
    for fz_key, fz_val in GenerateFuzzedHeaders():
      headers = {}
      headers.update(STATIC_RESPONSE_HEADERS)
      headers.update({fz_key: [fz_val]})
      json_headers = json.dumps(headers)
      b64_headers = base64.b64encode(json_headers)
      url = "http://%s/default?respBody=%s&respHeader=%s" % (TEST_SERVER,
        base64.b64encode(STATIC_RESPONSE_BODY), b64_headers)
      yield (json_headers, url)

  @Slow
  def testFuzzing(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      t.AddChromeArg('--data-reduction-proxy-http-proxies='
       'https://chromeproxy-test.appspot.com')
      for headers, url in self.GenerateTestURLs():
        try:
          t.LoadURL(url)
          # The main test is to make sure Chrome doesn't crash after loading a
          # page with fuzzed headers, which would be raised as a ChromeDriver
          # exception. Otherwise, we'll do a simple check and make sure the page
          # body is correct and Chrome isn't displaying some kind of error page.
          body = t.ExecuteJavascriptStatement('document.body.innerHTML')
          self.assertEqual(body, STATIC_RESPONSE_BODY)
        except Exception as e:
          print('Response headers: ' + headers)
          print('URL: ' + url)
          raise e

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
