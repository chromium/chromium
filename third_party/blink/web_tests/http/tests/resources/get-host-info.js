function get_host_info() {
  var ORIGINAL_HOST = '127.0.0.1';
  var REMOTE_HOST = 'localhost';
  // TODO(mkwst): This should be a host that WPT supports. I don't know why we're using 'example.test'.
  var OTHER_HOST = 'example.test';
  var HTTP_PORT = 8000;
  var HTTPS_PORT = 8443;
  try {
    // In W3C test, we can get the hostname and port number in config.json
    // using wptserve's built-in pipe.
    // http://wptserve.readthedocs.org/en/latest/pipes.html#built-in-pipes
    HTTP_PORT = eval('{{ports[http][0]}}');
    HTTPS_PORT = eval('{{ports[https][0]}}');
    ORIGINAL_HOST = eval('\'{{host}}\'');
    REMOTE_HOST = 'www1.' + ORIGINAL_HOST;
  } catch (e) {
  }
  return {
    HTTP_ORIGIN: 'http://' + ORIGINAL_HOST + ':' + HTTP_PORT,
    HTTPS_ORIGIN: 'https://' + ORIGINAL_HOST + ':' + HTTPS_PORT,
    HTTP_REMOTE_ORIGIN: 'http://' + REMOTE_HOST + ':' + HTTP_PORT,
    HTTPS_REMOTE_ORIGIN: 'https://' + REMOTE_HOST + ':' + HTTPS_PORT,
    UNAUTHENTICATED_ORIGIN: 'http://' + OTHER_HOST + ':' + HTTP_PORT,
    AUTHENTICATED_ORIGIN: 'https://' + OTHER_HOST + ':' + HTTPS_PORT
  };
}
