function grant_permission(feature_name, url) {
  if (window.testRunner) {
    testRunner.setPermission(
        feature_name, 'granted', url, location.origin);
  }
}

function assert_available_in_iframe(
    feature_name, test, location, expected, allow_attributes) {
  const frame = document.createElement('iframe');
  if (allow_attributes)
    frame.allow = allow_attributes;
  frame.src = location;

  grant_permission(feature_name, frame.src);

  window.addEventListener('message', test.step_func(evt => {
    if (evt.source == frame.contentWindow) {
      assert_equals(evt.data, expected);
      test.done();
    }
  }));

  document.body.appendChild(frame);
}

function run_permission_default_header_policy_tests(
    cross_origin, feature_name, allow_attributes, error_name,
    feature_promise_factory) {
  // This may be the version of the page loaded up in an iframe. If so, just
  // post the result of running the feature promise back to the parent.
  if (location.hash == '#iframe') {
    feature_promise_factory().then(
        () => window.parent.postMessage('#OK', '*'), error => {
          var name = error.name;
          // TODO(raymes): We use error.toString() here instead of error.name
          // because the latter currently returns undefined for PositionError.
          if (!name)
            name = error.toString().split(' ')[1].split(']')[0];
          window.parent.postMessage('#' + name, '*');
        });
    return;
  }

  grant_permission(feature_name, location.href);

  // Run the various tests.
  // 1. Top level frame.
  promise_test(
      () => feature_promise_factory(),
      'Default "' + feature_name +
          '" feature policy ["self"] allows the top-level document.');

  // 2. Same-origin iframe.
  // Append #iframe to the URL so we can detect the iframe'd version of the
  // page.
  const same_origin_frame_pathname = location.pathname + '#iframe';
  async_test(
      t => {
        assert_available_in_iframe(
            feature_name, t, same_origin_frame_pathname, '#OK');
      },
      'Default "' + feature_name +
          '" feature policy ["self"] allows same-origin iframes.');

  // 3. Cross-origin iframe.
  const cross_origin_frame_url = cross_origin + same_origin_frame_pathname;
  async_test(
      t => {
        assert_available_in_iframe(
            feature_name, t, cross_origin_frame_url, '#' + error_name);
      },
      'Default "' + feature_name +
          '" feature policy ["self"] disallows cross-origin iframes.');

  // 4. Cross-origin iframe with "allow" attribute.
  async_test(
      t => {
        assert_available_in_iframe(
            feature_name, t, cross_origin_frame_url, '#OK', allow_attributes);
      },
      'Feature policy "' + feature_name +
          '" can be enabled in cross-origin iframes using "allow" attribute.');
}
