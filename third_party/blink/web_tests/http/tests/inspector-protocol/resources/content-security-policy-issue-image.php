<?php
header("Content-Security-Policy: img-src 'self';");
?>
<!DOCTYPE html>
<html>
  <body>
    <h2>Webpage with blocked image source issue</h2>

    <div> <img src="https://thirdparty.test/network/resources/to-be-blocked.jpg" style="width:500px" alt="Image blocked|CSP violation"></div>
  </body>
</html>
