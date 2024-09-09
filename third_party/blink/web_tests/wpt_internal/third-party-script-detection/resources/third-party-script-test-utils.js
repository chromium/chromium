// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const targetScriptUrls = [
  // Word Press
  'https://c0.wp.com/c/6.4.2/wp-includes/js/dist/vendor/wp-polyfill.min.js',
  // Google Analytics
  'https://www.google-analytics.com/analytics.js',
  // Google Font Api
  'https://www.googleapis.com/example/webfont',
  // Meta Pixel
  'https://connect.facebook.net/en_US/fbevents.js',
  // Hotjar
  'https://static.hotjar.com/c/hotjar-1903348.js',
  // Elementor
  'https://elementor.com/wp-content/plugins/elementor/assets/lib/swiper/v8/swiper.min.js',
  // Slider Revolution
  'https://artfulagenda.com/wp-content/plugins/revslider/public/assets/js/jquery.themepunch.tools.min.js'
];

function third_party_script_test(test_type, url, expected_metric_value) {
  return promise_test(async t => {
    assert_implements(
      window.navigator.serviceWorker,
      'Service Worker is not supported.');
    assert_true(
      ['script-execution', 'script-callback'].includes(test_type),
      "Third party script test type must be one of 'script-execution' or 'script-callback'.");
    const script =
      test_type == 'script-execution'
        ? './resources/fetch-fake-long-3p-script-execution-sw.js'
        : './resources/fetch-fake-long-3p-script-callback-sw.js';
    const scope = './resources/fetch-fake-long-3p-script-frame.html';

    // Add service worker to this 1P context. wait_for_state() and
    // service_worker_unregister_and_register() are helper functions
    // for creating test ServiceWorkers defined in:
    // /service-workers/service-worker/resources/test-helpers.sub.js
    const reg = await service_worker_unregister_and_register(t, script, scope);
    t.add_cleanup(() => reg.unregister());
    await wait_for_state(t, reg.installing, 'activated');

    // Setup an iframe controlled by the service worker.
    const frame = await with_iframe(scope);
    t.add_cleanup(() => frame.remove());

    // Create a script element
    const scriptElement = frame.contentWindow.document.createElement('script');
    scriptElement.src = url;
    scriptElement.id = 'third-party-script-element-id';
    scriptElement.async = false;

    const scriptLoadPromise = new Promise(resolve => {
      scriptElement.addEventListener('load', resolve);
    });

    const recorder = internals.initializeUKMRecorder();
    // Add the script to the iframe's DOM
    frame.contentWindow.document.body.appendChild(scriptElement);

    await scriptLoadPromise
    await new Promise(resolve => requestAnimationFrame(() => {
      requestAnimationFrame(resolve);
    }));

    const entries = recorder.getMetrics(
      "PerformanceAPI.LongAnimationFrame",
      [
        "Categorized3PScriptLongAnimationFrameCallbackContributors",
        "Categorized3PScriptLongAnimationFrameScriptExecutionContributors"
      ]);

    // Entries usually have a length of 2, and the first entry will be an event
    // handler callback added by test-helpers.sub.js. However, when running
    // upstream on bots and repeating the test for times, starting from the
    // second time, the callback from test-helpers.sub.js is no longer
    // triggered, thus will get only 1 entry. As a result, we update this test
    // to accept flexible number of entries and always check the last entry
    // for the metric value.
    assert_greater_than_equal(entries.length, 1);
    const metrics = entries[entries.length - 1];
    assert_equals(
      metrics["Categorized3PScriptLongAnimationFrameCallbackContributors"],
      test_type == 'script-callback' ? expected_metric_value : 0);
    assert_equals(
      metrics["Categorized3PScriptLongAnimationFrameScriptExecutionContributors"],
      test_type == 'script-execution' ? expected_metric_value : 0);
  }, `${url} should be reported as a third party ${test_type} contributor in LoAF UKM.`);
}