<?php
header("Content-Security-Policy: script-src 'self';");
?>
<!DOCTYPE html>
<html>
  <body>
    <h2>Webpage with not allowed inline &lt;script&gt;</h2>

    <script>alert('Hello World!')</script>
  </body>
</html>
