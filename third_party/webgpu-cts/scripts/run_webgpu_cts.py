# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
from six.moves import BaseHTTPServer
from six.moves import urllib
import sys
from tempfile import mkstemp
import threading
import os
import logging

# This script is run via //third_party/blink/tools/run_webgpu_cts.py which
# adds blinkpy to the Python path.
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.web_tests import run_web_tests

path_finder.add_typ_dir_to_sys_path()
from typ.expectations_parser import TaggedTestListParser, Expectation
from typ.json_results import ResultType


# Basic HTTP request handler to serve the WebGPU webgpuCtsExpectations.js file
class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        BaseHTTPServer.BaseHTTPRequestHandler.__init__(self, *args, **kwargs)

    def do_GET(self):
        if self.path == "/webgpuCtsExpectations.js":
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Content-Type', 'application/javascript')
            self.end_headers()
            self.wfile.write(self.server.expectations_js.encode())
        elif self.path == '/_start' or self.path == '/_stop':
            self.send_response(200)
            self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write('Not found')


# Basic HTTP server which handles request using RequestHandler on a background thread.
class ExpectationsServer(BaseHTTPServer.HTTPServer):
    def __init__(self, expectations_js, server_address):
        BaseHTTPServer.HTTPServer.__init__(self, server_address,
                                           RequestHandler)
        self._should_run = False
        self._thread = None
        self.expectations_js = expectations_js

    def start(self):
        assert not self._thread

        self._should_run = True

        def _loop():
            while self._should_run:
                self.handle_request()

        self._thread = threading.Thread(name='webgpu_expectations_server',
                                        target=_loop)
        # Mark the thread as a daemon to be sure it exits. We still have an explicit
        # |stop| method because daemon threads are stopped abruptly at shutdown without
        # cleaning up resources.
        self._thread.daemon = True
        self._thread.start()

        # Ensure the server is running.
        # We want to wait synchronously for this so that server startup time doesn't
        # cut into test run time.
        while True:
            try:
                urllib.request.urlopen(
                    'http://%s:%d/_start' %
                    (self.server_address[0], self.server_address[1])).read()
            except IOError as e:
                logging.warning(e)
                continue
            return

    def stop(self):
        self._should_run = False
        try:
            # Load a url so |handle_request| returns.
            urllib.request.urlopen(
                'http://%s:%d/_stop' %
                (self.server_address[0], self.server_address[1])).read()
        except IOError as e:
            logging.warning(e)
        self._thread.join()
        self._thread = None


def split_cts_expectations_and_web_test_expectations(
        expectations_file_contents, platform_tags=None):
    """Split web test expectations (bit.ly/chromium-test-list-format) into a Javascript
    module containing expectations for the WebGPU CTS, and a filtered list of the same web
    test expectations, excluding the bits handled by the WebGPU CTS. Returns an object:
    {
      cts_expectations_js: "export const expectations = [ ... ]",
      web_test_expectations: {
        expectations: <expectations contents>
        tag_set: <frozenset of tags used by the expectations>
        result_set: <frozenset of result tags used by the expectations>
      }
    }"""

    cts_expectations = []

    out_tag_set = set()
    out_result_set = set()
    out_expectations = []

    parser = TaggedTestListParser(expectations_file_contents)

    # For each expectation, append it to |cts_expectations| if the CTS can understand it.
    # Expectations not supported by the CTS will be forwarded to the web tests harness.
    # This allows us to preserve expectations like [ Slow Crash Timeout RetryOnFailure ].
    # It also preserves expectations like [ Pass ] which are used for test splitting.
    # TODO(crbug.com/1186320): Handle test splits / variant generation separately?
    # Web test expectations that are passed through are run as separate variants.
    # Since [ Slow Crash Timeout RetryOnFailure Pass ] are Web test expectations,
    # they have the downside that they must be a prefix of the test name. If they don't match
    # anything the variant generator will warn.
    # TODO(crbug.com/1186320): Also validate the CTS expectation query.
    # TODO(crbug.com/1186320): We may be able to use skip expectations in the
    # CTS for Crash/Timeout, and then have a separate test suite which runs only the problematic
    # tests. We would generate variants specifically for each expectation to avoid the
    # prefix problem. This would allow us to have exact test suppressions at the cost of
    # potentially running some tests multiple times if there are overlapping expectations.
    for exp in parser.expectations:
        # Skip expectations that are not relevant to this platform
        if platform_tags is not None and not exp.tags.issubset(platform_tags):
            continue

        results = exp.results
        raw_results = exp.raw_results

        # Do not do special handling of expectations that aren't for the CTS.
        # ex.) ref tests run in WPT without the CTS.
        # TODO(crbug.com/1186320): This could be a more robust check.
        if 'q=webgpu:' in exp.test:
            # Pass Skip expectations to the CTS.
            if ResultType.Skip in results:
                assert len(
                    results
                ) == 1, 'Skip expectations must not be combined with other expectations'
                cts_expectations.append({
                    'query': exp.test,
                    'expectation': 'skip'
                })
                continue

            # Consume the [ Failure ] expectation for the CTS, but forward along other expectations.
            # [ Pass, Crash, Timeout ] will impact variant generation.
            # TODO(crbug.com/1186320): Teach the CTS RetryOnFailure.
            if ResultType.Failure in results and not exp.should_retry_on_failure:
                cts_expectations.append({
                    'query': exp.test,
                    'expectation': 'fail'
                })

                results = results.difference(set((ResultType.Failure, )))
                raw_results = [r for r in raw_results if r != 'Failure']

        if len(raw_results) != 0:
            # Forward everything, with the modified results.
            out_exp = Expectation(reason=exp.reason,
                                  test=exp.test,
                                  results=results,
                                  lineno=exp.lineno,
                                  retry_on_failure=exp.should_retry_on_failure,
                                  is_slow_test=exp.is_slow_test,
                                  conflict_resolution=exp.conflict_resolution,
                                  raw_tags=exp.raw_tags,
                                  raw_results=raw_results,
                                  is_glob=exp.is_glob,
                                  trailing_comments=exp.trailing_comments)

            out_expectations.append(out_exp)

            # Add the results and tags the expectation uses to sets.
            # We will prepend these to the top of the out file.
            out_result_set = out_result_set.union(out_exp.raw_results)
            out_tag_set = out_tag_set.union(out_exp.raw_tags)

    return {
        'cts_expectations_js':
        'export const expectations = ' + json.dumps(cts_expectations),
        'web_test_expectations': {
            'expectations': out_expectations,
            'tag_set': out_tag_set,
            'result_set': out_result_set
        }
    }


def main(args, stderr):
    parser = argparse.ArgumentParser(
        description=
        'Start the WebGPU expectations server, then forwards to run_web_tests.py'
    )
    parser.add_argument('--webgpu-cts-expectations', required=True)

    options, rest_args = parser.parse_known_args(args)

    web_test_expectations_fd, web_test_expectations_file = mkstemp()

    forwarded_args = rest_args + [
        '--ignore-default-expectations', '--additional-expectations',
        web_test_expectations_file
    ]

    run_web_tests_options = run_web_tests.parse_args(forwarded_args)[0]

    # Construct a web tests port using the test arguments forwarded to run_web_tests.py
    # (ex. --platform=android) in order to discover the tags that the web tests harness will
    # use. This includes the OS, OS version, architecture, etc.
    platform_tags = Host().port_factory.get(
        run_web_tests_options.platform,
        run_web_tests_options).get_platform_tags()

    with open(options.webgpu_cts_expectations) as f:
        split_result = split_cts_expectations_and_web_test_expectations(
            f.read(), platform_tags)

    # Write the out expectation file for web tests.
    with open(web_test_expectations_file, 'w') as expectations_out:
        web_test_exp = split_result['web_test_expectations']
        expectations_out.write('# tags: [ ' +
                               ' '.join(web_test_exp['tag_set']) + ' ]\n')
        expectations_out.write('# results: [ Slow ' +
                               ' '.join(web_test_exp['result_set']) + ' ]\n\n')
        for exp in web_test_exp['expectations']:
            expectations_out.write(exp.to_string() + '\n')

    server = ExpectationsServer(split_result['cts_expectations_js'],
                                ('127.0.0.1', 3000))

    logging.info('Starting expectations server...')
    server.start()

    try:
        return run_web_tests.main(forwarded_args, stderr)
    finally:
        logging.info('Stopping expectations server...')
        server.stop()
        os.close(web_test_expectations_fd)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stderr))
