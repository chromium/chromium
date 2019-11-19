# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import random
import re
import socket
import shlex
import subprocess
import sys
import tempfile
import time
import traceback
import unittest
import urlparse

from emulation_server import LocalEmulationServer

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
  os.pardir, 'third_party', 'webdriver', 'pylib'))
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

# Pretty printer for debug output.
def PrettyPrintJSON(obj):
  return json.dumps(obj, indent=2)

# These network condition values are used in SetNetworkConnection()
NETWORKS = {
    '4G': {
      'latency': 20,
      'upload_throughput': 4096 * 1024,
      'download_throughput': 4096 * 1024,
      'offline': False,
    },
    '3G': {
      'latency': 425,
      'upload_throughput': 750 * 1024,
      'download_throughput': 750 * 1024,
      'offline': False,
    },
    '2G': {
      'latency': 1650,
      'upload_throughput': 250 * 1024,
      'download_throughput': 250 * 1024,
      'offline': False,
    },
    'OFFLINE': {
      'latency': 0,
      'upload_throughput': 0 * 1024,
      'download_throughput': 0 * 1024,
      # This stays false so that Chrome won't disconnect from ChromeDriver.
      'offline': False,
    },
}

def ParseFlags():
  """Parses the given command line arguments.

  Returns:
    A new Namespace object with class properties for each argument added below.
    See pydoc for argparse.
  """
  def TestFilter(v):
    try:
      # The filtering here allows for any number of * wildcards with a required
      # . separator between classname and methodname, but no other special
      # characters.
      return re.match(r'^([A-Za-z_0-9\*]+\.)?[A-Za-z_0-9\*]+$', v).group(0)
    except:
      raise argparse.ArgumentTypeError('Test filter "%s" is not a valid filter'
        % v)
  parser = argparse.ArgumentParser()
  parser.add_argument('--browser_args', type=str, help='Override browser flags '
    'in code with these flags')
  parser.add_argument('--via_header_value',
    default='1.1 Chrome-Compression-Proxy', help='What the via should match to '
    'be considered valid')
  parser.add_argument('--aceh_header_value',
    default='Chrome-Proxy,Chrome-Proxy-Content-Transform,Via', help='Comma '
    'separated list of values that should be in the '
    'Access-Control-Expose-Headers header.')
  parser.add_argument('--android', help='If given, attempts to run the test on '
    'Android via adb. Ignores usage of --chrome_exec', action='store_true')
  parser.add_argument('--android_package',
    default='com.android.chrome', help='Set the android package for Chrome')
  parser.add_argument('--chrome_exec', type=str, help='The path to '
    'the Chrome or Chromium executable')
  parser.add_argument('chrome_driver', type=str, help='The path to '
    'the ChromeDriver executable. If not given, the default system chrome '
    'will be used.')
  parser.add_argument('--disable_buffer', help='Causes stdout and stderr from '
    'tests to output normally. Otherwise, the standard output and standard '
    'error streams are buffered during the test run, and output from a '
    'passing test is discarded. Output will always be echoed normally on test '
    'fail or error and is added to the failure messages.', action='store_true')
  parser.add_argument('-c', '--catch', help='Control-C during the test run '
    'waits for the current test to end and then reports all the results so '
    'far. A second Control-C raises the normal KeyboardInterrupt exception.',
    action='store_true')
  parser.add_argument('-f', '--failfast', help='Stop the test run on the first '
    'error or failure.', action='store_true')
  parser.add_argument('--test_filter', '--gtest_filter', type=TestFilter,
    help='The filter to use when discovering tests to run, in the form '
    '<class name>.<method name> Wildcards (*) are accepted. Default=*',
    default='*')
  parser.add_argument('--logging_level', choices=['DEBUG', 'INFO', 'WARN',
    'ERROR', 'CRIT'], default='WARN', help='The logging verbosity for log '
    'messages, printed to stderr. To see stderr logging output during a '
    'successful test run, also pass --disable_buffer. Default=ERROR')
  parser.add_argument('--log_file', help='If given, write logging statements '
    'to the given file instead of stderr.')
  parser.add_argument('--skip_slow', action='store_true', help='If set, tests '
    'marked as slow will be skipped.', default=False)
  parser.add_argument('--chrome_start_time', type=int, default=0, help='The '
    'number of attempts to check if Chrome has fetched a proxy client config '
    'before starting the test. Each check takes about one second.')
  parser.add_argument('--ignore_logging_prefs_w3c', action='store_true',
    help='If given, use the loggingPrefs capability instead of the W3C '
    'standard goog:loggingPrefs capability.')
  return parser.parse_args(sys.argv[1:])

def GetLogger(name='common'):
  """Creates a Logger instance with the given name and returns it.

  If a logger has already been created with the same name, that instance is
  returned instead.

  Args:
    name: The name of the logger to return.
  Returns:
    A logger with the given name.
  """
  logger = logging.getLogger(name)
  if hasattr(logger, "initialized"):
    return logger
  logging_level = ParseFlags().logging_level
  if logging_level == 'DEBUG':
    logger.setLevel(logging.DEBUG)
  elif logging_level == 'INFO':
    logger.setLevel(logging.INFO)
  elif logging_level == 'WARN':
    logger.setLevel(logging.WARNING)
  elif logging_level == 'ERROR':
    logger.setLevel(logging.ERROR)
  elif logging_level == 'CRIT':
    logger.setLevel(logging.CRITICAL)
  else:
    logger.setLevel(logging.NOTSET)
  formatter = logging.Formatter('%(asctime)s %(funcName)s() %(levelname)s: '
    '%(message)s')
  if ParseFlags().log_file:
    fh = logging.FileHandler(ParseFlags().log_file)
    fh.setFormatter(formatter)
    logger.addHandler(fh)
  else:
    ch = logging.StreamHandler(sys.stderr)
    ch.setFormatter(formatter)
    logger.addHandler(ch)
  logger.initialized = True
  return logger

def _RunAdbCmd(args):
  """Runs an adb command with the given arguments.

  Args:
    args: an array of string arguments
  """
  proc = subprocess.Popen(['adb'] + args, stdout=subprocess.PIPE,
    stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()
  if proc.returncode:
    raise Exception("ADB command failed. Output: %s" % (stdout + stderr))

def GetOpenPort():
    """Returns an open port on the host machine.

    Return:
      an open port number as an int
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('', 0))
    return int(sock.getsockname()[1])

class TestDriver:
  """The main driver for an integration test.

  This class is the tool that is used by every integration test to interact with
  the Chromium browser and validate proper functionality. This class sits on top
  of the Selenium Chrome Webdriver with added utility and helper functions for
  Chrome-Proxy. This class should be used with Python's 'with' operator.

  Attributes:
    _flags: A Namespace object from the call to ParseFlags()
    _driver: A reference to the driver object from the Chrome Driver library.
    _chrome_args: A set of string arguments to start Chrome with.
    _url: The string URL that Chrome will navigate to for this test.
    _has_logs: Boolean flag set when a page is loaded and cleared when logs are
      fetched.
    _control_network_connection: Boolean signal that this chromedriver instance
      was meant to support network connection control, and thus had to enable
      mobile emulation
    _network_connection: The connection type to use on start up
    _emulation_server: A reference to the emulation server being used
    _emulation_server_port: If this is not set to -1, the emulation server is
      being used for the test and is available on this port
    _enable_features: A string set of features to enable
    _disable_features: A string set of features to disable
  """

  def __init__(self, control_network_connection=False):
    self._flags = ParseFlags()
    self._driver = None
    self._chrome_args = set()
    # By default use the default_integration policy. It is the same as the
    # default policy except that it disables GFE caching to make sure we are
    # running the tests against the current server version.
    self._experiment = 'default_integration'
    self._url = ''
    self._logger = GetLogger(name='TestDriver')
    self._has_logs = False
    self._control_network_connection = control_network_connection
    self._net_log = None
    self._network_connection = None
    self._emulation_server = None
    self._emulation_server_port = -1
    self._enable_features = set()
    self._disable_features = set()
    # By default enable the NetworkService and DRPWithNetworkService. Individual
    # tests may override this behavior.
    self.EnableChromeFeature('NetworkService')
    self.EnableChromeFeature('DataReductionProxyEnabledWithNetworkService')

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, tb):
    if self._driver:
      self._StopDriver()
    if self._emulation_server:
      self._emulation_server.Shutdown()
      if self._flags.android:
        # Remove the Android port forwarding to the host machine.
        _RunAdbCmd(['reverse', '--remove',
          'tcp:%d' % self._emulation_server_port])
      self._emulation_server = None
      self._emulation_server_port = -1
    if self._net_log and self._flags.android:
      try:
        _RunAdbCmd('shell', 'rm', '-f', self._net_log)
      except:
        # Ignore errors, give only an attempt to rm the temp file
        pass
    if self._net_log and not self._flags.android:
      try:
        os.remove(self._net_log)
      except:
        # Ignore errors, give only an attempt to rm the temp file
        pass

  def _OverrideChromeArgs(self):
    """Overrides any given arguments in the code with those given on the command
    line.

    Arguments that need to be overridden may contain different values for
    a flag given in the code. In that case, check by the flag whether to
    override the argument.
    """
    # Set the Data Reduction Proxy experiment.
    if self._experiment is not None:
      self._chrome_args.add('--data-reduction-proxy-experiment=' +
        self._experiment)
    def GetDictKey(argument):
      return argument.split('=', 1)[0]
    if self._flags.browser_args and len(self._flags.browser_args) > 0:
      # Build a dict of flags mapped to the whole argument.
      original_args = {}
      for arg in self._chrome_args:
          original_args[GetDictKey(arg)] = arg
      # Override flags given in code with any command line arguments.
      for override_arg in shlex.split(self._flags.browser_args):
        arg_key = GetDictKey(override_arg)
        if (arg_key in original_args
              and original_args[arg_key] in self._chrome_args):
            self._chrome_args.remove(original_args[arg_key])
            self._logger.info('Removed Chrome flag. %s', original_args[arg_key])
        self._chrome_args.add(override_arg)
        self._logger.info('Added Chrome flag. %s', override_arg)
    # Always add the flag that allows histograms to be queried in javascript.
    self._chrome_args.add('--enable-stats-collection-bindings')

  def _StartDriver(self):
    """Parses the flags to pass to Chromium, then starts the ChromeDriver.

    If running Android, the Android package name is passed to ChromeDriver here.
    """
    self._OverrideChromeArgs()
    if self._emulation_server:
      self.AddChromeArg('--ignore-certificate-errors')
      self._emulation_server.StartAndReturn()
      if self._flags.android:
        # Forward the Android port to the host machine.
        address = 'tcp:%d' % self._emulation_server_port
        _RunAdbCmd(['reverse', address, address])

    capabilities = {}
    if self._flags.ignore_logging_prefs_w3c:
      capabilities = {'loggingPrefs': {'performance': 'INFO'}}
    else:
      capabilities = {'goog:loggingPrefs': {'performance': 'INFO'}}

    chrome_options = Options()

    if self._control_network_connection:
      capabilities.update({
        'networkConnectionEnabled': True,
        'mobileEmulationEnabled': True,
      })
      if not self._flags.android:
        chrome_options.add_experimental_option('mobileEmulation',
          {'deviceName': 'Pixel 2'})

    if len(self._enable_features) > 0:
      self._chrome_args.add(
        '--enable-features=%s' % ','.join(self._enable_features))
    if len(self._disable_features) > 0:
      self._chrome_args.add(
        '--disable-features=%s' % ','.join(self._disable_features))
    for arg in self._chrome_args:
      chrome_options.add_argument(arg)
    self._logger.info('Starting Chrome with these flags: %s',
      str(self._chrome_args))

    if self._flags.android:
      chrome_options.add_experimental_option('androidPackage',
        self._flags.android_package)
      self._logger.debug('Will use Chrome on Android')
    elif self._flags.chrome_exec:
      chrome_options.binary_location = self._flags.chrome_exec
      self._logger.info('Using the Chrome binary at this path: %s',
        self._flags.chrome_exec)

    self._logger.debug('ChromeOptions will be parsed into these capabilities: '
      '%s', PrettyPrintJSON(chrome_options.to_capabilities()))

    driver = webdriver.Chrome(executable_path=self._flags.chrome_driver,
      desired_capabilities=capabilities, chrome_options=chrome_options)
    driver.command_executor._commands.update({
      'getAvailableLogTypes': ('GET', '/session/$sessionId/log/types'),
      'getLog': ('POST', '/session/$sessionId/log'),
      'setNetworkConditions':
        ('POST', '/session/$sessionId/chromium/network_conditions')})
    self._driver = driver

    if self._control_network_connection:
      # Set network connection if it was called before LoadURL()
      self.SetNetworkConnection(self._network_connection)

    self.SleepUntilHistogramHasEntry(
      'DataReductionProxy.ConfigService.FetchResponseCode',
      sleep_intervals=self._flags.chrome_start_time)

  def _StopDriver(self):
    """Nicely stops the ChromeDriver.
    """
    self._logger.debug('Stopping ChromeDriver')
    self._driver.quit()
    self._driver = None

  def SetExperiment(self, exp):
    """Sets the Data Reduction Proxy experiment to use.

    Args:
      exp: a string with the experiment name.
    """
    self._experiment = exp

  def AddChromeArgs(self, args):
    """Adds multiple arguments that will be passed to Chromium at start.

    Args:
      args: An iterable of strings, each an argument to pass to Chrome at start.
    """
    for arg in args:
      self._chrome_args.add(arg)

  def AddChromeArg(self, arg):
    """Adds a single argument that will be passed to Chromium at start.

    Args:
      arg: a string argument to pass to Chrome at start
    """
    if '--enable-features' in arg:
      raise Exception('AddChromeArg("--enable-features=Foo" is not supported. '
        'Please use EnableChromeFeature("Foo") instead.')
    if '--disable-features' in arg:
      raise Exception('AddChromeArg("--disable-features=Foo" is not supported. '
        'Please use DisableChromeFeature("Foo") instead.')

    self._chrome_args.add(arg)
    self._logger.debug('Adding Chrome arg: %s', arg)

  def EnableChromeFeature(self, feature):
    """Adds a single feature flag to add to `--enable-features`.

    Args:
      feature: the Chrome feature to enable
    """
    self._enable_features.add(feature)
    if feature in self._disable_features:
      self._disable_features.remove(feature)

  def DisableChromeFeature(self, feature):
    """Adds a single feature flag to add to `--disable-features`.

    Args:
      feature: the Chrome feature to enable
    """
    self._disable_features.add(feature)
    if feature in self._enable_features:
      self._enable_features.remove(feature)

  def RemoveChromeArgs(self, args):
    """Removes multiple arguments that will no longer be passed to Chromium at
    start.

    Args:
        args: An iterable of strings to no longer use the next time Chrome
          starts.
    """
    for arg in args:
      self._chrome_args.discard(arg)

  def RemoveChromeArg(self, arg):
    """Removes a single argument that will no longer be passed to Chromium at
    start.

    Args:
      arg: A string flag to no longer use the next time Chrome starts.
    """
    self._chrome_args.discard(arg)
    self._logger.debug('Removing Chrome arg: %s', arg)

  def ClearChromeArgs(self):
    """Removes all arguments from Chromium at start.
    """
    self._chrome_args.clear()
    self._logger.debug('Clearing all Chrome args')

  def ClearCache(self):
    """Clears the browser cache.

    Important note: ChromeDriver automatically starts
    a clean copy of Chrome on every instantiation.
    """
    res = self.ExecuteJavascript('if(window.chrome && chrome.benchmarking && '
      'chrome.benchmarking.clearCache){chrome.benchmarking.clearCache(); '
      'chrome.benchmarking.clearPredictorCache();chrome.benchmarking.'
      'clearHostResolverCache();}')
    self._logger.info('Cleared browser cache. Returned=%s', str(res))

  def UseNetLog(self):
    """Requests that a Chrome netlog be available for test evaluation.
    """
    if self._driver:
      raise Exception("UseNetLog() must be called before LoadURL()")
    temp_basename = "chrome.netlog.%05d.json" % random.randint(1, 100000)
    temp_dir = tempfile.gettempdir()
    if self._flags.android:
      temp_dir = '/data/local/tmp'
    temp_file = os.path.join(temp_dir, temp_basename)
    if self._flags.android:
      _RunAdbCmd(['shell', 'touch', temp_file])
    self.AddChromeArg('--log-net-log=%s' % temp_file)
    self._net_log = temp_file

  def UseEmulationServer(self, handler, port=None):
    """Requests the test driver to use the emulation server.

    Args:
      port: The port to run the server on.
      handler: The handler to use, subclassed from BaseRequestHandler.
    """
    if not port:
      port = GetOpenPort()
    self._emulation_server = LocalEmulationServer(port, handler)
    self._emulation_server_port = port

  def SetNetworkConnection(self, connection_type):
    """Changes the emulated connection type.

    Args:
      connection_type: the connection type to use according to the dict near the
        top of the file OR a dictionary specifying the network conditions
    """
    if not self._control_network_connection:
      raise Exception('SetNetworkConnection can only be used with a TestDriver '
        'initalized with control_network_connection=True')
    self._network_connection = connection_type
    network = (NETWORKS[self._network_connection]
      if connection_type in NETWORKS else connection_type)
    if self._driver and self._network_connection:
      self._driver.execute('setNetworkConditions',
        {'network_conditions': network})

  def LoadURL(self, url, timeout=30):
    """Starts Chromium with any arguments previously given and navigates to the
    given URL.

    Args:
      url: The URL to navigate to.
      timeout: The time in seconds to load the page before timing out.
    """
    # Reduce some flakiness by checking if we should ensure the proxy has fully
    # initialized first.
    if not self._driver:
      self._StartDriver()

    disabled_config_service = False
    for arg in self._chrome_args:
      if 'DataReductionProxyConfigService/Disabled' in arg:
        disabled_config_service = True
    if (not disabled_config_service and
        '--enable-spdy-proxy-auth' in self._chrome_args and
        'DataReductionProxyEnabledWithNetworkService' in self._enable_features):
      self._driver.get('data:,')
      self.SleepUntilHistogramHasEntry(
        'DataReductionProxy.ConfigService.FetchResponseCode', sleep_intervals=5)
    self._url = url
    if (len(urlparse.urlparse(url).netloc) == 0 and
        len(urlparse.urlparse(url).scheme) == 0):
      self._logger.warn('Invalid URL: "%s". Did you forget to prepend '
        '"http://"? See RFC 1808 for more information', url)
    self._driver.set_page_load_timeout(timeout)
    self._logger.debug('Set page load timeout to %f seconds', timeout)
    self._driver.get(self._url)
    self._logger.debug('Loaded page %s', url)
    self._has_logs = True

  def FindElement(self, by, value):
    """Finds an element on the page.

    Uses the By selector and value given.
    Args:
      by: the selenium.webdriver.common.By selector
      value: the value
    Returns:
      a WebElement object
    """
    return self._driver.find_element(by=by, value=value)

  def ExecuteJavascript(self, script, timeout=30):
    """Executes the given javascript in the browser's current page in an
    anonymous function.

    If you expect a result and don't get one, try adding a return statement or
    using ExecuteJavascriptStatement() below.

    Args:
      script: A string of Javascript code.
      timeout: Timeout for the Javascript code to return in seconds.
    Returns:
      A string of the verbatim output from the Javascript execution.
    """
    if not self._driver:
      self._StartDriver()
    # TODO(robertogden): Use 'driver.set_script_timeout(timeout)' instead after
    # crbug/672114 is fixed.
    default_timeout = socket.getdefaulttimeout()
    socket.setdefaulttimeout(timeout)
    self._logger.debug('Set socket timeout to %f seconds', timeout)
    script_result = self._driver.execute_script(script)
    self._logger.debug('Executed Javascript in browser: %s', script)
    socket.setdefaulttimeout(default_timeout)
    self._logger.debug('Set socket timeout to %s', str(default_timeout))
    return script_result

  def ExecuteJavascriptStatement(self, script, timeout=30):
    """Wraps ExecuteJavascript() for use with a single statement.

    Behavior is analogous to 'function(){ return <script> }();'

    Args:
      script: A string of Javascript code.
      timeout: Timeout for the Javascript code to return in seconds.
    Returns:
      A string of the verbatim output from the Javascript execution.
    """
    return self.ExecuteJavascript("return " + script, timeout)

  def GetHistogram(self, histogram, timeout=30):
    """Gets a Chrome histogram as a dictionary object.

    Args:
      histogram: the name of the histogram to fetch
      timeout: timeout for the underlying Javascript query.

    Returns:
      A dictionary object containing information about the histogram.
    """
    js_query = 'statsCollectionController.getHistogram("%s")' % histogram
    string_response = self.ExecuteJavascriptStatement(js_query, timeout)
    self._logger.debug('Got %s histogram=%s', histogram, string_response)
    return json.loads(string_response)

  def GetBrowserHistogram(self, histogram, timeout=30):
    """Gets a Chrome histogram as a dictionary object from browser process.

    Args:
      histogram: the name of the histogram to fetch
      timeout: timeout for the underlying Javascript query.

    Returns:
      A dictionary object containing information about the histogram.
    """
    js_query = 'statsCollectionController.getBrowserHistogram("%s")' % histogram
    string_response = self.ExecuteJavascriptStatement(js_query, timeout)
    self._logger.debug('Got %s histogram=%s', histogram, string_response)
    return json.loads(string_response)

  def WaitForJavascriptExpression(self, expression, timeout, min_poll=0.1,
      max_poll=1):
    """Waits for the given Javascript expression to evaluate to True within the
    given timeout. This method polls the Javascript expression within the range
    of |min_poll| and |max_poll|.

    Args:
      expression: The Javascript expression to poll, as a string.
      min_poll: The most frequently to poll as a float.
      max_poll: The least frequently to poll as a float.
    Returns: The result of the expression.
    """
    poll_interval = max(min(max_poll, float(timeout) / 10.0), min_poll)
    self._logger.debug('Poll interval=%f seconds', poll_interval)
    result = self.ExecuteJavascriptStatement(expression)
    total_waited_time = 0
    while not result and total_waited_time < timeout:
      time.sleep(poll_interval)
      total_waited_time += poll_interval
      result = self.ExecuteJavascriptStatement(expression)
    if not result:
      self._logger.error('%s not true after %f seconds' % (expression, timeout))
      raise Exception('%s not true after %f seconds' % (expression, timeout))
    return result

  def StopAndGetNetLog(self):
    """Stops the browser and returns the parsed net log.

    Must be called after UseNetLog(). Will attempt to fix an unfinished netlog
    dump if initial parse fails.

    Returns: the parsed netlog dict object
    """
    if self._driver:
      self._StopDriver()
      # Give a moment for Chrome to close and finish writing the netlog.
      time.sleep(5)
    if not self._net_log:
      raise Exception('GetParsedNetLog() cannot be called before UseNetLog()')
    temp_file = self._net_log
    if self._flags.android:
      temp_file = os.path.join(tempfile.gettempdir(), 'pulled_netlog.json')
      _RunAdbCmd(['pull', self._net_log, temp_file])
    json_file_content = ''
    with open(temp_file) as f:
      json_file_content = f.read()
    try:
      return json.loads(json_file_content)
    except:
      # Using --log-net-log does not guarantee a valid json file. Workaround
      # copied from
      # https://cs.chromium.org/chromium/src/third_party/catapult/netlog_viewer/netlog_viewer/log_util.js?l=275&rcl=017fd5cf4ccbcbed7bba20760f1b3d923a7cd3ca
      end = max(json_file_content.rfind(',\n'), json_file_content.rfind(',\r'))
      if end == -1:
        raise Exception('unable to parse netlog json file')
      json_file_content = json_file_content[:end] + ']}'
      return json.loads(json_file_content)

  def GetPerformanceLogs(self, method_filter=r'Network\.(requestWillBeSent|' +
                                                        'responseReceived)'):
    """Returns all logged Performance events from Chrome. Raises an Exception if
    no pages have been loaded since the last time this function was called.

    Args:
      method_filter: A regex expression to match the method of logged events
        against. Only logs who's method matches the regex will be returned.
    Returns:
      Performance logs as a list of dicts, since the last time this function was
      called.
    """
    if not self._has_logs:
      raise Exception('No pages loaded since last Network log query!')
    all_messages = []
    for log in self._driver.execute('getLog', {'type': 'performance'})['value']:
      message = json.loads(log['message'])['message']
      self._logger.debug('Got Performance log:\n%s', PrettyPrintJSON(message))
      if re.match(method_filter, message['method']):
        all_messages.append(message)
    self._logger.info('Got %d performance logs with filter method=%s',
      len(all_messages), method_filter)
    self._has_logs = False
    return all_messages

  def SleepUntilHistogramHasEntry(self, histogram_name, sleep_intervals=10):
    """Polls if a histogram exists in 1-6 second intervals for 10 intervals.
    Allows script to run with a timeout of 5 seconds, so the default behavior
    allows up to 60 seconds until timeout.

    Args:
      histogram_name: The name of the histogram to wait for
      sleep_intervals: The number of polling intervals, each polling cycle is 1s
    Returns:
      Whether the histogram exists
    """
    while (sleep_intervals > 0):
      if (self.GetHistogram(histogram_name, 3) or
          self.GetBrowserHistogram(histogram_name, 3)):
        return True
      time.sleep(1)
      sleep_intervals -= 1

    return False

  def GetHTTPResponses(self, include_favicon=False, skip_domainless_pages=True,
      override_has_logs=False):
    """Parses the Performance Logs and returns a list of HTTPResponse objects.

    Use caution when calling this function  multiple times. Only responses
    since the last time this function was called are returned (or since Chrome
    started, whichever is later). An Exception will be raised if no page was
    loaded since the last time this function was called.

    Args:
      include_favicon: A bool that if True will include responses for favicons.
      skip_domainless_pages: If True, only responses with a net_loc as in RFC
        1808 will be included. Pages such as about:blank will be skipped.
      override_has_logs: Allows the _has_logs property to be set if there was
        not a page load but an XHR was expected instead.
    Returns:
      A list of HTTPResponse objects, each representing a single completed HTTP
      transaction by Chrome.
    """
    if override_has_logs:
      self._has_logs = True

    all_requests = {}  # map from requestId to params
    all_responses = [] # list of HTTPResponse

    def MakeHTTPResponse(message):
      params = message['params']
      response_dict = params['response'] if 'response' in params else {}
      http_response_dict = {
        'response_headers': response_dict['headers'] if 'headers' in
          response_dict else {},
        'request_headers': response_dict['requestHeaders'] if 'requestHeaders'
          in response_dict else {},
        'url': response_dict['url'] if 'url' in response_dict else '',
        'protocol': response_dict['protocol'] if 'protocol' in response_dict
          else '',
        'port': response_dict['remotePort'] if 'remotePort' in response_dict
          else -1,
        'status': response_dict['status'] if 'status' in response_dict else -1,
        'request_type': params['type'] if 'type' in params else '',
        'redirect_chain': [],
      }
      if params['requestId'] in all_requests:
        for request in all_requests[params['requestId']][:-1]:
          http_response_dict['redirect_chain'].append(request['request']['url'])
      return HTTPResponse(**http_response_dict)

    for message in self.GetPerformanceLogs():
      if message['method'] == 'Network.requestWillBeSent':
        requestId = message['params']['requestId']
        if requestId not in all_requests:
          all_requests[requestId] = [message['params']]
        else:
          all_requests[requestId].append(message['params'])
        continue
      response = MakeHTTPResponse(message)
      self._logger.debug('New HTTPResponse: %s', str(response))
      is_favicon = response.url.endswith('favicon.ico')
      has_domain = len(urlparse.urlparse(response.url).netloc) > 0
      if (not is_favicon or include_favicon) and (not skip_domainless_pages or
          has_domain):
        all_responses.append(response)
      else:
        self._logger.info("Skipping HTTPResponse with url=%s in returned logs.",
          response.url)
    self._logger.info('%d new HTTPResponse objects found in the logs %s '
      'favicons', len(all_responses), ('including' if include_favicon else
      'not including'))
    return all_responses

class HTTPResponse:
  """This class represents a single HTTP transaction (request and response) by
  Chrome.

  This class also includes several convenience functions for ChromeProxy
  specific assertions.

  Attributes:
    _response_headers: A dict of response headers.
    _request_headers: A dict of request headers.
    _url: the fetched url
    _protocol: The protocol used to get the response.
    _port: The remote port number used to get the response.
    _status: The integer status code of the response
    _request_type: What caused this request (Document, XHR, etc)
    _flags: A Namespace object from ParseFlags()
  """

  def __init__(self, response_headers, request_headers, url, protocol, port,
      status, request_type, redirect_chain):
    self._response_headers = {}
    self._request_headers = {}
    self._url = url
    self._protocol = protocol
    self._port = port
    self._status = status
    self._request_type = request_type
    self._redirect_chain = redirect_chain # empty if no redirects
    self._flags = ParseFlags()
    # Make all header names lower case.
    for name in response_headers:
      self._response_headers[name.lower()] = response_headers[name]
    for name in request_headers:
      self._request_headers[name.lower()] = request_headers[name]

  def __str__(self):
    return PrettyPrintJSON({
      'response_headers': self._response_headers,
      'request_headers': self._request_headers,
      'url': self._url,
      'protocol': self._protocol,
      'port': self._port,
      'status': self._status,
      'request_type': self._request_type,
      'redirect_chain': self._redirect_chain,
    })

  @property
  def response_headers(self):
    return self._response_headers

  @property
  def request_headers(self):
    return self._request_headers

  @property
  def url(self):
    return self._url

  @property
  def protocol(self):
    return self._protocol

  @property
  def port(self):
    return self._port

  @property
  def status(self):
    return self._status

  @property
  def request_type(self):
    return self._request_type

  @property
  def redirect_chain(self):
    return self._redirect_chain

  def ResponseHasViaHeader(self):
    return 'via' in self._response_headers and (self._flags.via_header_value in
      [piece.strip() for piece in self._response_headers['via'].split(',')])

  def WasXHR(self):
    return self.request_type == 'XHR'

  def UsedHTTP(self):
    return self._protocol == 'http/1.1'

  def UsedHTTP2(self):
    return self._protocol == 'h2'

class IntegrationTest(unittest.TestCase):
  """This class adds ChromeProxy-specific assertions to the generic
  unittest.TestCase class.
  """

  def assertHasProxyHeaders(self, http_response):
    """Asserts that Proxy headers are present contain the expected values
    as provided on the command line.

    The Proxy adds two headers: Via, and Access-Control-Expose-Headers.
    This checks that both headers contain the values enumerated in both
    --via_header_value and --aceh_header_value. Both headers are additive,
    so there may be more values in the headers than expected.

    Args:
      http_response: The HTTPResponse object to check.
    """
    # Via header check
    self.assertIn('via', http_response.response_headers)
    expected_via_header = ParseFlags().via_header_value.lower()
    actual_via_headers = set(
      [h.lower() for h in http_response.response_headers['via'].split(',')])
    self.assertIn(expected_via_header, actual_via_headers, "Via header not in "
      "response headers! Expected: %s, Actual: %s" %
      (expected_via_header, actual_via_headers))

    # Access-Control-Expose-Headers header check.
    if ParseFlags().aceh_header_value:
      aceh = 'access-control-expose-headers'
      self.assertIn(aceh,  http_response.response_headers)
      expected_aceh_header = set(
          [h.lower() for h in ParseFlags().aceh_header_value.split(',')])
      actual_aceh_header = set(
          [h.lower() for h in http_response.response_headers[aceh].split(',')])
      diff = expected_aceh_header - actual_aceh_header
      self.assertTrue(len(diff) == 0, "Access-Control-Expose-Headers missing "
        "values (%s)! Expected: [%s], Actual: [%s]" %
        (",".join(diff), ",".join(expected_aceh_header),
         ",".join(actual_aceh_header)))

  def assertNotHasChromeProxyViaHeader(self, http_response):
    """Asserts that the Via header in the given HTTPResponse does not match the
    expected value as given on the command line.

    Args:
      http_response: The HTTPResponse object to check.
    """
    if 'via' in http_response.response_headers:
      expected_via_header = ParseFlags().via_header_value
      actual_via_headers = http_response.response_headers['via'].split(',')
      self.assertNotIn(expected_via_header, actual_via_headers, "Via header "
        "found in response headers! Not expected: %s, Actual: %s" %
        (expected_via_header, actual_via_headers))

  def determinePreviewShownViaHistogram(self, test_driver, preview_type):
    """Determines if the preview type was shown by histogram check.

    Checks both InfoBar and Android Omnibox histograms for the given type.

    Args:
      test_driver: The TestDriver instance.
      preview_type: The preview type to check for.

    Returns:
      Whether a preview UI was displayed.
    """
    infobar_histogram_name = 'Previews.InfoBarAction.%s' % preview_type
    android_histogram_name = 'Previews.OmniboxAction.%s' % preview_type
    testing_histogram_name = 'Previews.PreviewShown.%s' % preview_type

    infobar_histogram = test_driver.GetBrowserHistogram(
      infobar_histogram_name, 5)
    android_histogram = test_driver.GetBrowserHistogram(
      android_histogram_name, 5)
    testing_histogram = test_driver.GetBrowserHistogram(
      testing_histogram_name, 5)

    count = sum([
      infobar_histogram.get('count', 0),
      android_histogram.get('count', 0),
      testing_histogram.get('count', 0),
    ])
    return count > 0

  def assertPreviewShownViaHistogram(self, test_driver, preview_type):
    """Asserts that |determinePreviewShownViaHistogram| returns true.

    Args:
      test_driver: The TestDriver instance.
      preview_type: The preview type to check for.
    """
    self.assertTrue(
      self.determinePreviewShownViaHistogram(test_driver, preview_type))

  def assertPreviewNotShownViaHistogram(self, test_driver, preview_type):
    """Asserts that |determinePreviewShownViaHistogram| returns false.

    Args:
      test_driver: The TestDriver instance.
      preview_type: The preview type to check for.
    """
    self.assertFalse(
      self.determinePreviewShownViaHistogram(test_driver, preview_type))

  def checkLoFiResponse(self, http_response, expected_lo_fi):
    """Asserts that if expected the response headers contain the Lo-Fi directive
    then the request headers do too. If the CPAT header contains if-heavy, the
    request should not be LoFi. If-heavy will be deprecated in the future. Also
    checks that the content size is less than 100 if |expected_lo_fi|.
    Otherwise, checks that the response and request headers don't contain the
    Lo-Fi directive and the content size is greater than 100.

    Args:
      http_response: The HTTPResponse object to check.
      expected_lo_fi: Whether the response should be Lo-Fi.

    Returns:
      Whether the response was Lo-Fi.
    """

    if (expected_lo_fi) :
      self.assertHasProxyHeaders(http_response)
      content_length = http_response.response_headers['content-length']
      cpat_request = http_response.request_headers[
                       'chrome-proxy-accept-transform']
      cpct_response = http_response.response_headers[
                        'chrome-proxy-content-transform']
      if ('empty-image' in cpct_response):
        self.assertIn('empty-image', cpat_request)
        self.assertLess(int(content_length), 100)
        return True;
      return False;
    else:
      if ('chrome-proxy-accept-transform' in http_response.request_headers):
        cpat_request = http_response.request_headers[
                       'chrome-proxy-accept-transform']
        if ('empty-image' in cpat_request):
          self.assertIn('if-heavy', cpat_request)
      self.assertNotIn('chrome-proxy-content-transform',
        http_response.response_headers)
      content_length = http_response.response_headers['content-length']
      self.assertGreater(int(content_length), 100)
      return False;

  def checkLitePageResponse(self, http_response):
    """Asserts that if the response headers contain the Lite Page directive then
    the request headers do too.

    Args:
      http_response: The HTTPResponse object to check.

    Returns:
      Whether the response was a Lite Page.
    """

    self.assertHasProxyHeaders(http_response)
    if ('chrome-proxy-content-transform' not in http_response.response_headers):
      return False;
    cpct_response = http_response.response_headers[
                      'chrome-proxy-content-transform']
    cpat_request = http_response.request_headers[
                      'chrome-proxy-accept-transform']
    if ('lite-page' in cpct_response):
      self.assertIn('lite-page', cpat_request)
      return True;
    return False;

  @staticmethod
  def RunAllTests(run_all_tests=False):
    """A simple helper method to run all tests using unittest.main().

    Args:
      run_all_tests: If True, all tests in the directory will be run, Otherwise
        only the tests in the file given on the command line will be run.
    Returns:
      the TestResult object from the test runner
    """
    flags = ParseFlags()
    logger = GetLogger()
    logger.debug('Command line args: %s', str(sys.argv))
    logger.info('sys.argv parsed to %s', str(flags))
    if flags.catch:
      unittest.installHandler()
    # Use python's test discovery to locate tests files that have subclasses of
    # unittest.TestCase and methods beginning with 'test'.
    pattern = '*.py' if run_all_tests else os.path.basename(sys.argv[0])
    loader = unittest.TestLoader()
    test_suite_iter = loader.discover(os.path.dirname(__file__),
      pattern=pattern)
    # Match class and method name on the given test filter from --test_filter.
    tests = unittest.TestSuite()
    test_filter_re = flags.test_filter.replace('.', r'\.').replace('*', '.*')
    for test_suite in test_suite_iter:
      for test_case in test_suite:
        for test in test_case:
          # Drop the file name in the form <filename>.<classname>.<methodname>
          test_id = test.id()[test.id().find('.') + 1:]
          if re.match(test_filter_re, test_id):
            tests.addTest(test)
    testRunner = unittest.runner.TextTestRunner(verbosity=2,
      failfast=flags.failfast, buffer=(not flags.disable_buffer))
    return testRunner.run(tests)
