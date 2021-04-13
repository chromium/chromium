// This test code is shared between resource-timing-sizes-redirect.html and
// resource-timing-sizes-redirect-worker.html

if (typeof document === 'undefined') {
  importScripts('/resources/testharness.js',
    '/resources/get-host-info.js?pipe=sub',
    '/misc/resources/run-async-tasks-promise.js');
}

const baseUrl =
  new URL('/security/resources/cors-hello.php', location.href).href;
const expectedSize = 73;

// Because apache decrements the Keep-Alive max value on each request, the
// transferSize will vary slightly between requests for the same resource.
const fuzzFactor = 3;  // bytes

const minHeaderSize = 100;

const hostInfo = get_host_info();

var directUrl, sameOriginRedirect, crossOriginRedirect, mixedRedirect;
var complexRedirect;

function checkBodySizeFields(entry) {
  assert_equals(entry.decodedBodySize, expectedSize, 'decodedBodySize');
  assert_equals(entry.encodedBodySize, expectedSize, 'encodedBodySize');
}

function checkResourceSizes() {
  var entries = performance.getEntriesByType('resource');
  var lowerBound, upperBound, withRedirectLowerBound;
  var seenCount = 0;
  for (var entry of entries) {
    switch (entry.name) {
      case directUrl:
        checkBodySizeFields(entry);
        assert_greater_than(entry.transferSize, expectedSize,
          'transferSize');
        lowerBound = entry.transferSize - fuzzFactor;
        upperBound = entry.transferSize + fuzzFactor;
        withRedirectLowerBound = entry.transferSize + minHeaderSize;
        ++seenCount;
        break;

      case sameOriginRedirect:
        checkBodySizeFields(entry);
        assert_greater_than(entry.transferSize, withRedirectLowerBound,
          'transferSize');
        ++seenCount;
        break;

      case crossOriginRedirect:
      case mixedRedirect:
      case complexRedirect:
        checkBodySizeFields(entry);
        assert_between_exclusive(entry.transferSize, lowerBound, upperBound,
          'transferSize');
        ++seenCount;
        break;

      default:
        break;
    }
  }
  assert_equals(seenCount, 5, 'seenCount');
}

function redirectUrl(redirectSourceOrigin, allowOrigin, targetUrl) {
  return redirectSourceOrigin +
    '/resources/redirect.php?cors_allow_origin=' +
    encodeURIComponent(allowOrigin) +
    '&url=' + encodeURIComponent(targetUrl) +
    '&timing_allow_origin=*';
}

promise_test(() => {
  // Use a different URL every time so that the cache behaviour does not
  // depend on execution order.
  directUrl = baseUrl + '?unique=' + Math.random().toString().substring(2) +
    '&cors=*';
  sameOriginRedirect = redirectUrl(hostInfo.HTTP_ORIGIN, '*', directUrl);
  crossOriginRedirect = redirectUrl(hostInfo.HTTP_REMOTE_ORIGIN,
    hostInfo.HTTP_ORIGIN, directUrl);
  mixedRedirect = redirectUrl(hostInfo.HTTP_REMOTE_ORIGIN,
    hostInfo.HTTP_ORIGIN, sameOriginRedirect);
  complexRedirect = redirectUrl(hostInfo.HTTP_ORIGIN,
    hostInfo.HTTP_REMOTE_ORIGIN, mixedRedirect);
  var eatBody = response => response.arrayBuffer();
  return fetch(directUrl)
    .then(eatBody)
    .then(() => fetch(sameOriginRedirect))
    .then(eatBody)
    .then(() => fetch(crossOriginRedirect))
    .then(eatBody)
    .then(() => fetch(mixedRedirect))
    .then(eatBody)
    .then(() => fetch(complexRedirect))
    .then(eatBody)
    .then(runAsyncTasks)
    .then(checkResourceSizes);
}, 'PerformanceResourceTiming sizes Fetch with redirect test');

done();
