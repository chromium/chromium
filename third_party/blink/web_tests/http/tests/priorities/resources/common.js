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

function openWindow(url) {
  const win = window.open(url, '_blank');
  add_result_callback(() => win.close());
}

function resource_load_priority_test(windowURL, expected_priority,
                                     description) {
  promise_test(async () => {
    openWindow('resources/' + windowURL);

    // The order in which these two events are sent can't be relied upon.
    const priority_event_promise =
       new Promise(resolve => window.onRequestPriorityUpdated = resolve);
    const subresource_finished_loading_event_promise =
       new Promise(resolve => window.onRequestStatusChanged = resolve);

    const priority_event = await priority_event_promise;
    assert_equals(priority_event, expected_priority);

    const subresource_finished_loading_event =
        await subresource_finished_loading_event_promise;
    assert_equals(subresource_finished_loading_event, 'LOADED',
                  'The resource loaded successfully');

  }, description);
}

function observeAndReportResourceLoadPriority(url, optionalDoc, message) {
  const documentToUse = optionalDoc ? optionalDoc : document;
  return internals.getInitialResourcePriority(url, documentToUse)
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
