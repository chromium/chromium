#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dromaeo benchmark automation script.

Script runs dromaeo tests in browsers specified by --browser switch and saves
results to a spreadsheet on docs.google.com.

Prerequisites:
1. Install Google Data APIs Python Client Library from
   http://code.google.com/p/gdata-python-client.
2. Checkout Dromaeo benchmark from
   http://src.chromium.org/svn/trunk/src/chrome/test/data/dromaeo and provide
   local path to it in --dromaeo_home switch.
3. Create a spreadsheet at http://docs.google.com and specify its name in
   --spreadsheet switch

Benchmark results are presented in the following format:
browser | date time
test 1 name|m11|...|m1n|test 1 average mean| |e11|...|e1n|test 1 average error
test 2 name|m21|...|m2n|test 2 average mean| |e21|...|e2n|test 2 average error
...

Here mij is mean run/s in individual dromaeo test i during benchmark run j,
eij is error in individual dromaeo test i during benchmark run j.

Example usage:
dromaeo_benchmark_runner.py -b "E:\chromium\src\chrome\Release\chrome.exe"
    -b "C:\Program Files (x86)\Safari\safari.exe"
    -b "C:\Program Files (x86)\Opera 10.50 pre-alpha\opera.exe" -n 1
    -d "E:\chromium\src\chrome\test\data\dromaeo" -f dom -e example@gmail.com

"""

from __future__ import print_function

import getpass
import json
import os
import re
import subprocess
import time
import urlparse
from optparse import OptionParser
from BaseHTTPServer import HTTPServer
import SimpleHTTPServer
import gdata.spreadsheet.service

max_spreadsheet_columns = 20
test_props = ['mean', 'error']


def ParseArguments():
  parser = OptionParser()
  parser.add_option("-b", "--browser",
                    action="append", dest="browsers",
                    help="list of browsers to test")
  parser.add_option("-n", "--run_count", dest="run_count", type="int",
                    default=5, help="number of runs")
  parser.add_option("-d", "--dromaeo_home", dest="dromaeo_home",
                    help="directory with your dromaeo files")
  parser.add_option("-p", "--port", dest="port", type="int",
                    default=8080, help="http server port")
  parser.add_option("-f", "--filter", dest="filter",
                    default="dom", help="dromaeo suite filter")
  parser.add_option("-e", "--email", dest="email",
                    help="your google docs account")
  parser.add_option("-s", "--spreadsheet", dest="spreadsheet_title",
                    default="dromaeo",
                    help="your google docs spreadsheet name")

  options = parser.parse_args()[0]

  if not options.dromaeo_home:
    raise Exception('please specify dromaeo_home')

  return options


def KillProcessByName(process_name):
  process = subprocess.Popen('wmic process get processid, executablepath',
                             stdout=subprocess.PIPE)
  stdout = str(process.communicate()[0])
  match = re.search(re.escape(process_name) + '\s+(\d+)', stdout)
  if match:
    pid = match.group(1)
    subprocess.call('taskkill /pid %s' % pid)


class SpreadsheetWriter(object):
  "Utility class for storing benchmarking results in Google spreadsheets."

  def __init__(self, email, spreadsheet_title):
    '''Login to google docs and search for spreadsheet'''

    self.token_file = os.path.expanduser("~/.dromaeo_bot_auth_token")
    self.gd_client = gdata.spreadsheet.service.SpreadsheetsService()

    authenticated = False
    if os.path.exists(self.token_file):
      token = ''
      try:
        file = open(self.token_file, 'r')
        token = file.read()
        file.close()
        self.gd_client.SetClientLoginToken(token)
        self.gd_client.GetSpreadsheetsFeed()
        authenticated = True
      except (IOError, gdata.service.RequestError):
        pass
    if not authenticated:
      self.gd_client.email = email
      self.gd_client.password = getpass.getpass('Password for %s: ' % email)
      self.gd_client.source = 'python robot for dromaeo'
      self.gd_client.ProgrammaticLogin()
      token = self.gd_client.GetClientLoginToken()
      try:
        file = open(self.token_file, 'w')
        file.write(token)
        file.close()
      except (IOError):
        pass
      os.chmod(self.token_file, 0600)

    # Search for the spreadsheet with title = spreadsheet_title.
    spreadsheet_feed = self.gd_client.GetSpreadsheetsFeed()
    for spreadsheet in spreadsheet_feed.entry:
      if spreadsheet.title.text == spreadsheet_title:
        self.spreadsheet_key = spreadsheet.id.text.rsplit('/', 1)[1]
    if not self.spreadsheet_key:
      raise Exception('Spreadsheet %s not found' % spreadsheet_title)

    # Get the key of the first worksheet in spreadsheet.
    worksheet_feed = self.gd_client.GetWorksheetsFeed(self.spreadsheet_key)
    self.worksheet_key = worksheet_feed.entry[0].id.text.rsplit('/', 1)[1]

  def _InsertRow(self, row):
    row = dict([('c' + str(i), row[i]) for i in xrange(len(row))])
    self.gd_client.InsertRow(row, self.spreadsheet_key, self.worksheet_key)

  def _InsertBlankRow(self):
    self._InsertRow('-' * self.columns_count)

  def PrepareSpreadsheet(self, run_count):
    """Update cells in worksheet topmost row with service information.

    Calculate column count corresponding to run_count and create worksheet
    column titles [c0, c1, ...] in the topmost row to speed up spreadsheet
    updates (it allows to insert a whole row with a single request)
    """

    # Calculate the number of columns we need to present all test results.
    self.columns_count = (run_count + 2) * len(test_props)
    if self.columns_count > max_spreadsheet_columns:
      # Google spreadsheet has just max_spreadsheet_columns columns.
      max_run_count = max_spreadsheet_columns / len(test_props) - 2
      raise Exception('maximum run count is %i' % max_run_count)
    # Create worksheet column titles [c0, c1, ..., cn].
    for i in xrange(self.columns_count):
      self.gd_client.UpdateCell(1, i + 1, 'c' + str(i), self.spreadsheet_key,
                                self.worksheet_key)

  def WriteColumnTitles(self, run_count):
    "Create titles for test results (mean 1, mean 2, ..., average mean, ...)"
    row = []
    for prop in test_props:
      row.append('')
      for i in xrange(run_count):
        row.append('%s %i' % (prop, i + 1))
      row.append('average ' + prop)
    self._InsertRow(row)

  def WriteBrowserBenchmarkTitle(self, browser_name):
    "Create browser benchmark title (browser name, date time)"
    self._InsertBlankRow()
    self._InsertRow([browser_name, time.strftime('%d.%m.%Y %H:%M:%S')])

  def WriteBrowserBenchmarkResults(self, test_name, test_data):
    "Insert a row with single test results"
    row = []
    for prop in test_props:
      if not row:
        row.append(test_name)
      else:
        row.append('')
      row.extend([str(x) for x in test_data[prop]])
      row.append(str(sum(test_data[prop]) / len(test_data[prop])))
    self._InsertRow(row)


class DromaeoHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def do_POST(self):
    self.send_response(200)
    self.end_headers()
    self.wfile.write("<HTML>POST OK.<BR><BR>");
    length = int(self.headers.getheader('content-length'))
    parameters = urlparse.parse_qs(self.rfile.read(length))
    self.server.got_post = True
    self.server.post_data = parameters['data']


class BenchmarkResults(object):
  "Storage class for dromaeo benchmark results"

  def __init__(self):
    self.data = {}

  def ProcessBrowserPostData(self, data):
    "Convert dromaeo test results in internal format"
    tests = json.loads(data[0])
    for test in tests:
      test_name = test['name']
      if test_name not in self.data:
        # Test is encountered for the first time.
        self.data[test_name] = dict([(prop, []) for prop in test_props])
      # Append current run results.
      for prop in test_props:
        value = -1
        if prop in test: value = test[prop] # workaround for Opera 10.5
        self.data[test_name][prop].append(value)


def main():
  options = ParseArguments()

  # Start sever with dromaeo.
  os.chdir(options.dromaeo_home)
  server = HTTPServer(('', options.port), DromaeoHandler)

  # Open and prepare spreadsheet on google docs.
  spreadsheet_writer = SpreadsheetWriter(options.email,
                                         options.spreadsheet_title)
  spreadsheet_writer.PrepareSpreadsheet(options.run_count)
  spreadsheet_writer.WriteColumnTitles(options.run_count)

  for browser in options.browsers:
    browser_name = os.path.splitext(os.path.basename(browser))[0]
    spreadsheet_writer.WriteBrowserBenchmarkTitle(browser_name)
    benchmark_results = BenchmarkResults()
    for run_number in xrange(options.run_count):
      print('%s run %i' % (browser_name, run_number + 1))
      # Run browser.
      test_page = 'http://localhost:%i/index.html?%s&automated&post_json' % (
        options.port, options.filter)
      browser_process = subprocess.Popen('%s "%s"' % (browser, test_page))
      server.got_post = False
      server.post_data = None
      # Wait until POST request from browser.
      while not server.got_post:
        server.handle_request()
      benchmark_results.ProcessBrowserPostData(server.post_data)
      # Kill browser.
      KillProcessByName(browser)
      browser_process.wait()

    # Insert test results into spreadsheet.
    for (test_name, test_data) in benchmark_results.data.iteritems():
      spreadsheet_writer.WriteBrowserBenchmarkResults(test_name, test_data)

  server.socket.close()
  return 0


if __name__ == '__main__':
  sys.exit(main())
