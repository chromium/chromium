<?php
header("Content-Security-Policy: script-src 'self' 'unsafe-inline';");
?>
<!DOCTYPE html>
<script>
new Worker('content-security-policy-worker-import.js', {type: 'module'});
</script>
