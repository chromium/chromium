#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Issue a series of GetHash requests to the SafeBrowsing servers and measure
the response times.

Usage:

  $ ./gethash_timer.py --period=600 --samples=20 --output=resp.csv

  --period (or -p):  The amount of time (in seconds) to wait between GetHash
                     requests. Using a value of more than 300 (5 minutes) to
                     include the effect of DNS.

  --samples (or -s): The number of requests to issue. If this parameter is not
                     specified, the test will run indefinitely.

  --output (or -o):  The path to a file where the output will be written in
                     CSV format: sample_number,response_code,elapsed_time_ms
"""

from __future__ import print_function

import getopt
import httplib
import sys
import time

_GETHASH_HOST = 'safebrowsing.clients.google.com'
_GETHASH_REQUEST = (
    '/safebrowsing/gethash?client=googleclient&appver=1.0&pver=2.1')

# Global logging file handle.
g_file_handle = None


def IssueGetHash(prefix):
  '''Issue one GetHash request to the safebrowsing servers.
  Args:
    prefix: A 4 byte value to look up on the server.
  Returns:
    The HTTP response code for the GetHash request.
  '''
  body = '4:4\n' + prefix
  h = httplib.HTTPConnection(_GETHASH_HOST)
  h.putrequest('POST', _GETHASH_REQUEST)
  h.putheader('content-length', str(len(body)))
  h.endheaders()
  h.send(body)
  response_code = h.getresponse().status
  h.close()
  return response_code


def TimedGetHash(prefix):
  '''Measure the amount of time it takes to receive a GetHash response.
  Args:
    prefix: A 4 byte value to look up on the the server.
  Returns:
    A tuple of HTTP resonse code and the response time (in milliseconds).
  '''
  start = time.time()
  response_code = IssueGetHash(prefix)
  return response_code, (time.time() - start) * 1000


def RunTimedGetHash(period, samples=None):
  '''Runs an experiment to measure the amount of time it takes to receive
  multiple responses from the GetHash servers.

  Args:
    period:  A floating point value that indicates (in seconds) the delay
             between requests.
    samples: An integer value indicating the number of requests to make.
             If 'None', the test continues indefinitely.
  Returns:
    None.
  '''
  global g_file_handle
  prefix = '\x50\x61\x75\x6c'
  sample_count = 1
  while True:
    response_code, elapsed_time = TimedGetHash(prefix)
    LogResponse(sample_count, response_code, elapsed_time)
    sample_count += 1
    if samples is not None and sample_count == samples:
      break
    time.sleep(period)


def LogResponse(sample_count, response_code, elapsed_time):
  '''Output the response for one GetHash query.
  Args:
    sample_count:  The current sample number.
    response_code: The HTTP response code for the GetHash request.
    elapsed_time:  The round-trip time (in milliseconds) for the
                   GetHash request.
  Returns:
    None.
  '''
  global g_file_handle
  output_list = (sample_count, response_code, elapsed_time)
  print('Request: %d, status: %d, elapsed time: %f ms' % output_list)
  if g_file_handle is not None:
    g_file_handle.write(('%d,%d,%f' % output_list) + '\n')
    g_file_handle.flush()


def SetupOutputFile(file_name):
  '''Open a file for logging results.
  Args:
    file_name: A path to a file to store the output.
  Returns:
    None.
  '''
  global g_file_handle
  g_file_handle = open(file_name, 'w')


def main():
  period = 10
  samples = None

  options, args = getopt.getopt(sys.argv[1:],
                                's:p:o:',
                                ['samples=', 'period=', 'output='])
  for option, value in options:
    if option == '-s' or option == '--samples':
      samples = int(value)
    elif option == '-p' or option == '--period':
      period = float(value)
    elif option == '-o' or option == '--output':
      file_name = value
    else:
      print('Bad option: %s' % option)
      return 1
  try:
    print('Starting Timed GetHash ----------')
    SetupOutputFile(file_name)
    RunTimedGetHash(period, samples)
  except KeyboardInterrupt:
    pass

  print('Timed GetHash complete ----------')
  g_file_handle.close()


if __name__ == '__main__':
  sys.exit(main())
