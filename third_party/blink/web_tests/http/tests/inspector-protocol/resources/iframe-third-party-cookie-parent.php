<?php
if(isset($_GET['opaque'])) {
  header("Content-Security-Policy: sandbox;");
}
?>
<iframe src="https://example.test:8443/inspector-protocol/resources/iframe-third-party-cookie-child.php"></iframe>
