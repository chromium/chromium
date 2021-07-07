<?php
  header("Cross-Origin-Opener-Policy: same-origin");
  header("Cross-Origin-Embedder-Policy: require-corp");
?><!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
</head>
<body>
<script>
worker = startWorker("resources/global-constructors-attributes-worker.js.php");
</script>
</body>
</html>
