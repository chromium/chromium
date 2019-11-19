/**
 * These values map to some of the the current
 * priority enum members in blink::ResourceLoadPriority.
 * The values are exposed through window.internals
 * and in these tests, we use the below variables to represent
 * the exposed values in a readable way.
 */
const kVeryLow = 0,
      kLow = 1,
      kMedium = 2,
      kHigh = 3,
      kVeryHigh = 4;

function observeAndReportResourceLoadPriority(url, optionalDoc, message) {
  const documentToUse = optionalDoc ? optionalDoc : document;
  return internals.getResourcePriority(url, documentToUse)
    .then(reportPriority)
}

function reportPriority(priority) {
  window.opener.postMessage({'Priority': priority}, '*');
}

function reportLoaded() {
  window.opener.postMessage({'Status': 'LOADED'}, '*');
}

function reportFailure() {
  window.opener.postMessage({'Status': 'FAILED'}, '*');
}
