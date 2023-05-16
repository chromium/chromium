function checkSameOriginDictionaryFetchHeaders(fetch_header_response) {
  assert_equals(fetch_header_response['status'], 'OK',
    'Dictionary shall be fetched.');
  request_header = fetch_header_response['request_header']
  assert_equals(request_header['sec-fetch-mode'], 'cors',
    'Dictionary shall set the CORS header.');
  assert_equals(request_header['sec-fetch-site'], 'same-origin',
    'sec-fetch-site header shall be same-origin.');
}

function checkRemoteOriginDictionaryFetchHeaders(fetch_header_response) {
  assert_equals(fetch_header_response['status'], 'OK',
    'Dictionary shall be fetched.');
  request_header = fetch_header_response['request_header']
  assert_equals(request_header['sec-fetch-mode'], 'cors',
    'Dictionary fetch shall set the CORS header.');
  assert_equals(request_header['sec-fetch-site'], 'same-site',
    'sec-fetch-site header shall be same-site.');
  assert_equals(request_header['origin'], get_host_info().HTTPS_ORIGIN,
    'origin header shall be set to the page origin.');
}

function checkNotSameSiteDictionaryFetchHeaders(fetch_header_response) {
  assert_equals(fetch_header_response['status'], 'OK',
    'Dictionary shall be fetched.');
  request_header = fetch_header_response['request_header']
  assert_equals(request_header['sec-fetch-mode'], 'cors',
    'Dictionary fetch shall set the CORS header.');
  assert_equals(request_header['sec-fetch-site'], 'cross-site',
    'sec-fetch-site header shall be cross-site.');
  assert_equals(request_header['origin'], get_host_info().HTTPS_ORIGIN,
    'origin header shall be set to the page origin.');
}
