<?php
header("Content-Security-Policy: require-trusted-types-for 'script';");
?>
<!DOCTYPE html>
<html>
  <body>
    <div id="exampleDiv"></div>
    <script>
      document.getElementById("exampleDiv").innerHTML = "foo";
    </script>
  </body>
</html>
