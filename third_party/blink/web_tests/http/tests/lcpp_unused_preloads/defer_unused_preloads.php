<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {mojo} from "/gen/mojo/public/js/bindings.js";
import {NonAssociatedWebTestControlHostRemote} from "/gen/content/web_test/common/web_test.mojom.m.js";
import {ByteString} from "/gen/mojo/public/mojom/base/byte_string.mojom.m.js";
import {LCPCriticalPathPredictorNavigationTimeHint} from "/gen/third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.m.js";

if (!window.testRunner) {
  console.log("This test requires window.testRunner.")
}

testRunner.dumpAsText();
testRunner.waitUntilDone();
if (window.location.search != "?start") {
  const hint = new LCPCriticalPathPredictorNavigationTimeHint();

  const resp = await fetch("/gen/third_party/blink/renderer/core/lcp_critical_path_predictor/test_proto/lcp_image_id.pb");

  hint.unusedPreloads = [
    {url: new URL('/resources/square.png', location).toString()},
  ];
  // All fields are non-nullable.
  hint.lcpElementLocators = [];
  hint.lcpInfluencerScripts = [];
  hint.preconnectOrigins = [];
  hint.fetchedFonts = [];

  const web_test_control_host_remote = new NonAssociatedWebTestControlHostRemote();
  web_test_control_host_remote.$.bindNewPipeAndPassReceiver().bindInBrowser('process');
  web_test_control_host_remote.setLCPPNavigationHint(hint);

  window.location.search = '?start';
}
</script>
<?php
// Do not output the HTML below this PHP block until the test is reloaded with
// "?start" to avoid it being picked up by the HTMLPreloadScanner.
if ($_SERVER['QUERY_STRING'] != "start")
  exit;
?>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<link rel="preload" href="/resources/square.png" as="image" onload="onLoadPreload()">

<body>
<script>
  var is_loaded = false;
  let promise = new Promise(resolve => {
    window.onLoadPreload = () => {
      is_loaded = true;
      resolve();
    }
  });

  const appendImage = async () => {
    return new Promise(resolve => {
      const img = document.createElement('img');
      img.onload = resolve;
      document.body.appendChild(img);
      img.src = "/resources/square20.jpg";
    });
  };

  promise_test(() => {
    const url = new URL('/resources/squre.png', location).toString();
    assert_false(is_loaded, "The loading is not started yet")
    return appendImage()
      .then(() => {
        assert_false(is_loaded, "The preload is still not loaded after subsequent resource was already loaded.");
      })
      .then(() => {
        return new Promise(resolve => {
          window.addEventListener('load', () => {
            assert_false(is_loaded, "preload is still not loaded in window.onload");
            resolve();
          });
        })
      })
      .then(promise);
  }, "Ensure LCPP hinted unused preloads were deferred by the timing predictor.");
</script>
</body>
