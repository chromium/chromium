<?php
  header("Cross-Origin-Opener-Policy: same-origin");
  header("Cross-Origin-Embedder-Policy: require-corp");
?><!DOCTYPE html>
<script src="/js-test-resources/js-test.js"></script>
<script>
description("This test documents all interface attributes and methods on SharedWorkerGlobalScope.");
worker = startWorker("resources/global-interface-listing-worker.js.php", "shared");
worker.port.postMessage({ platformSpecific: true });
</script>
