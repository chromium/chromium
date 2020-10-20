<?php
header("Content-Security-Policy: script-src 'self' 'unsafe-inline';");
?>
<!DOCTYPE html>
<html>
  <body>
    <h2>Webpage with not allowed eval()</h2>

    <script>alert(eval('7+10'))</script>
  </body>
</html>
