<?php
if(isset($_GET['opaque'])) {
  header("Content-Security-Policy: sandbox;");
}
?>
<?php if(isset($_GET['opaque'])): ?>
  <iframe src="https://example.test:8443/inspector-protocol/resources/iframe-third-party-cookie-child.php?opaque"></iframe>
<?php else: ?>
  <iframe src="https://example.test:8443/inspector-protocol/resources/iframe-third-party-cookie-child.php"></iframe>
<?php endif; ?>
