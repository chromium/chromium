<?php
header("Content-Security-Policy-Report-Only: script-src 'unsafe-inline'; style-src https://thirdparty.test/network/resources/;");
?>
<!DOCTYPE html>
<html>
  <link rel="stylesheet" type="text/css" href="style.css">
  <body>
    <script>eval('7+10')</script>
    <img src="to-be-allowed.jpg" style="width:500px" alt="Image blocked|CSP violation">
  </body>
</html>
