<title>BackForwardCacheExperimentHTTPHeader - enabled by origin trial</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<div id="status"></div>
<script>
test(t => {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'resources/echo-request-sec-bfcache-experiment.php');
  xhr.onload = function (e) {
    document.getElementById('status').innerText =  xhr.responseText;
    if (window.testRunner)
      testRunner.notifyDone();
  };
  xhr.onerror = function (e) {
    document.getElementById('status').innerText = "FAIL";
    if (window.testRunner)
      testRunner.notifyDone();
  };
  xhr.send();
}, 'Sec-bfcache-experiment must be set.');
</script>
</body>
