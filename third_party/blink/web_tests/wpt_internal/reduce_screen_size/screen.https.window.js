// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/common/subset-tests-by-key.js
// META: variant=?include=denied
// META: variant=?include=granted

// Testing the impact of the `ReduceScreenSize` flag.

// Opens a new 300x400 window positioned at (100,200), and wait for its notification.
async function open_window(test) {
  let popup_promise = async _ => {
    return new Promise(resolve => {
      let w = window.open("/html/browsers/windows/resources/post-to-opener.html", "_blank", "popup, left=100, top=200, width=300, height=400");
      window.addEventListener('message', e => {
        if (e.source === w) {
          assert_equals(w.innerWidth, 300, "Inner Width");
          assert_equals(w.innerHeight, 400, "Inner Height");
          assert_equals(typeof w.screen, "object", "Screen available.");

          resolve(w);
        }
      });
    });
  };
  const popup = await popup_promise();

  test.add_cleanup(() => {
    popup.close();
  });

  return popup;
}

const FLAG_ENABLED = true;
const FLAG_DISABLED = false;

const PERMISSION_GRANTED = 'granted';
const PERMISSION_DENIED = 'denied';

const EXPECT_REDUCTION = true;
const EXPECT_NO_REDUCTION = false;

function generate_test(flagState, permissionState, expectReduction) {
  subsetTestByKey(permissionState, promise_test, async t => {
    // Set the runtime flag here so it will take effect when the popup loads:
    internals.runtimeFlags.reduceScreenSizeEnabled = flagState;

    // Open the popup for testing:
    let popup = await open_window(t);

    const permission = await navigator.permissions.query({ name: "window-management" });
    assert_equals(permission.state, permissionState);

    if (expectReduction === EXPECT_REDUCTION) {
      // When reducing the available information:
      //
      // 1. Screen heights and widths should match the window's height
      //    and width.
      assert_equals(popup.screen.width, popup.innerWidth, "Width");
      assert_equals(popup.screen.height, popup.innerHeight, "Height");
      assert_equals(popup.screen.availWidth, popup.innerWidth, "Avail Width");
      assert_equals(popup.screen.availHeight, popup.innerHeight, "Avail Height");
      assert_true(popup.matchMedia(`(device-width: ${popup.innerWidth}px)`).matches, "Device Width")
      assert_true(popup.matchMedia(`(device-height: ${popup.innerHeight}px)`).matches, "Device Height");

      // 2. Windows should all appear positioned at the top-left corner.
      assert_equals(popup.screenX, 0, "screenX");
      assert_equals(popup.screenLeft, 0, "screenLeft");
      assert_equals(popup.screenY, 0, "screenY");
      assert_equals(popup.screenTop, 0, "screenY");

      // 3. No information about secondary screens should be available.
      assert_false(popup.screen.isExtended, "isExtended");

      // 4. Color/pixel depth is 24.
      assert_equals(popup.screen.colorDepth, 24, "Color Depth");
      assert_equals(popup.screen.pixelDepth, 24, "Pixel Depth");

      // 5. Color Gamut is `srgb`:
      assert_true(popup.matchMedia(`(color-gamut: srgb)`).matches, "Color Gamut");
    } else {
      // Height/Width
      assert_not_equals(popup.screen.width, popup.innerWidth, "Width");
      assert_not_equals(popup.screen.height, popup.innerHeight, "Height");
      assert_not_equals(popup.screen.availWidth, popup.innerWidth, "Avail Width");
      assert_not_equals(popup.screen.availHeight, popup.innerHeight, "Avail Height");

      // If we're not reducing size information, then `device-width` will be
      // larger than `innerWidth`, so asking for a minimum width that's just wider will pass.
      assert_true(popup.matchMedia(`(min-device-width: ${popup.innerWidth + 1}px)`).matches, "Device Width");
      assert_true(popup.matchMedia(`(min-device-height: ${popup.innerHeight + 1}px)`).matches, "Device Height");

      // TODO: `window.screen*` doesn't work in content_shell (and we force content_shell
      // for `wpt_internal` tests).
      //
      // TODO: There doesn't seem to be any way to pretend that `isExtended` should
      // be true, which makes it hard to test in its unreduced state.
      //
      // TODO: There doesn't seem to be any way to pretend that the color depth/gamut is
      // anything other than the default value.
    }
  }, `Flag ${flagState ? "enabled" : "disabled"}, ` +
     `Permission ${permissionState}: ` +
     `${expectReduction ? "reduced" : "unreduced"}`);
}

// (Ab)using the variant mechanism to set the permission status for the
// relevant tests. Since it's global state, setting it from within each
// test would otherwise risk stomping on other test's expectations:
//
// Permission Granted
//
subsetTestByKey("granted", promise_test, async _ => {
  return test_driver.set_permission({name: 'window-management'}, 'granted');
  const permission = await navigator.permissions.query({ name: "window-management" });
  assert_equals(permission.state, 'granted');
}, "Precondition: Grant permission.");

generate_test(FLAG_DISABLED, PERMISSION_GRANTED, EXPECT_NO_REDUCTION);
generate_test(FLAG_ENABLED, PERMISSION_GRANTED, EXPECT_NO_REDUCTION);

//
// Permission Denied
//
subsetTestByKey("denied", promise_test, async _ => {
  return test_driver.set_permission({name: 'window-management'}, 'denied');
  const permission = await navigator.permissions.query({ name: "window-management" });
  assert_equals(permission.state, 'denied');
}, "Precondition: Deny permission.");

generate_test(FLAG_DISABLED, PERMISSION_DENIED, EXPECT_NO_REDUCTION);
generate_test(FLAG_ENABLED, PERMISSION_DENIED, EXPECT_REDUCTION);
