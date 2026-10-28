<?php
header("Content-Security-Policy: img-src 'self';");
?>
<!DOCTYPE html>
<html>
  <style>
    .blocked {
      width: 100px;
      height: 100px;
      background-image: url(https://thirdparty.test/network/resources/to-be-blocked.jpg);
    }
  </style>
  <body>
    <div id="target"></div>
  </body>
</html>
