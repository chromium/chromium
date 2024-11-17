<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {setupLCPTest} from "../lcp_critical_path_predictor/resources/common.js";
await setupLCPTest({
  unusedPreloads : [
    {url: new URL('/resources/square.png', location).toString()},
  ]
});
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
