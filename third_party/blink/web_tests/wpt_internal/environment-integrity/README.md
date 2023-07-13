# Web Environment Integrity

These tests are for the WEI API proposed in
https://github.com/RupertBenWiser/Web-Environment-Integrity/blob/main/explainer.md

## Running these tests

Please note that these tests rely on
platform specific behaviour on Android while we prototype them
and so they are not running against any platform.

If we can enable these tests for all platforms, we should remove
the environment-integrity entry from `NeverFixTests`.

You can manually run them using run_wpt_test.py.

Eg for TrichromeWebView64:

```sh
# First build your WebView
...
./third_party/blink/tools/run_wpt_tests.py -p webview -t <TARGET> --webview-provider ./out/<TARGET>/apks/TrichromeWebView64.apk --browser-apk ./out/<TARGET>/apks/SystemWebViewShell.apk -- wpt_internal/environment-integrity/platform-agnostic.https.html
```
