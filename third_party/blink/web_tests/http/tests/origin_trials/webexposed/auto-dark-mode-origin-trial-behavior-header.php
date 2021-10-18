<?php
// Generate token with the command:
// generate_token.py http://127.0.0.1:8000 AutoDarkMode --expire-timestamp=2000000000

header("Origin-Trial: AyWCm6J71a4pwPzRIUOrEfpDSlIjoONkqThP5D2UOXaZUutVQAD3RfGQ6VCfkIiwrRSafaB2Pa64M+6g0HHnUg0AAABUeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQXV0b0RhcmtNb2RlIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
?>
<!doctype html>
<meta charset="utf-8">
<title>Auto Dark Mode - behavior imposed by the origin trial (HTTP header)</title>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<div id="detection" style="background-color: canvas; color-scheme: light"></div>
<script>
  test(test => {
    // Auto Dark Mode can be detected by adjusted values for system color names,
    // such as the "canvas" value used in this example, which defaults to white:
    // https://www.w3.org/TR/css-color-4/#valdef-system-color-canvas
    assert_not_equals(
      getComputedStyle(detection).backgroundColor, 'rgb(255, 255, 255)');

  }, "The Auto Dark Mode Origin Trial alters coloring of the Web content");
</script>
