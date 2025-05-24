// Testing the impact of the `ReduceScreenSize` flag.

// Opens a new 300x400 window to `about:blank`, positioned at (100,200).
function open_window(test) {
  let popup = window.open("", "_blank", "popup, left=100, top=200, width=300, height=400");

  assert_equals(popup.innerWidth, 300, "Inner Width");
  assert_equals(popup.innerHeight, 400, "Inner Height");

  test.add_cleanup(() => {
    popup.close();
  });

  return popup;
}

test(t => {
  let popup = open_window(t);
  assert_not_equals(popup.screen.width, popup.innerWidth, "Width");
  assert_not_equals(popup.screen.height, popup.innerHeight, "Height");
  assert_not_equals(popup.screen.availWidth, popup.innerWidth, "Avail Width");
  assert_not_equals(popup.screen.availHeight, popup.innerHeight, "Avail Height");

  // Media Queries
  //
  // If we're not reducing size information, then `device-width` will be
  // larger than `innerWidth`, so asking for a minimum width that's just wider will pass.
  assert_true(popup.matchMedia(`(min-device-width: ${popup.innerWidth + 1}px)`).matches, "Device Width");
  assert_true(popup.matchMedia(`(min-device-height: ${popup.innerHeight + 1}px)`).matches, "Device Height");

  // `window.screen*` doesn't work in content_shell (and we force content_shell
  // for `wpt_internal` tests).
  //
  // assert_equals(popup.screenX, 100, "screenX");
  // assert_equals(popup.screenLeft, 100, "screenLeft");
  // assert_equals(popup.screenY, 200, "screenY");
  // assert_equals(popup.screenTop, 200, "screenY");
  //
  // TODO: There doesn't seem to be any way to pretend that `isExtended` should
  // be true, which makes it hard to test in its unreduced state.
  //
  // TODO: There doesn't seem to be any way to pretend that the color depth is
  // anything other than the default value.
}, "Verify default behavior.");

test(t => {
  internals.runtimeFlags.reduceScreenSizeEnabled = true;

  let popup = open_window(t);
  assert_equals(popup.screen.width, popup.innerWidth, "Width");
  assert_equals(popup.screen.height, popup.innerHeight, "Height");
  assert_equals(popup.screen.availWidth, popup.innerWidth, "Avail Width");
  assert_equals(popup.screen.availHeight, popup.innerHeight, "Avail Height");

  assert_equals(popup.screenX, 0, "screenX");
  assert_equals(popup.screenLeft, 0, "screenLeft");
  assert_equals(popup.screenY, 0, "screenY");
  assert_equals(popup.screenTop, 0, "screenY");

  assert_false(popup.screen.isExtended, "isExtended");

  assert_equals(popup.screen.colorDepth, 24, "Color Depth");
  assert_equals(popup.screen.pixelDepth, 24, "Pixel Depth");

  // Media Queries
  //
  // If we're reducing size information, then `device-width` will be
  // `innerWidth`, so asking for a minimum width that's just wider will fail.
  assert_false(popup.matchMedia(`(min-device-width: ${popup.innerWidth + 1}px)`).matches, "Device Width")
  assert_false(popup.matchMedia(`(min-device-height: ${popup.innerHeight + 1}px)`).matches, "Device Height");
}, "Verify reduced behavior.");
