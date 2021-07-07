<?php
  header("Cross-Origin-Opener-Policy: same-origin");
  header("Cross-Origin-Embedder-Policy: require-corp");
?><!DOCTYPE html>
<script src="/js-test-resources/js-test.js"></script>
<script>
description("This test documents all interface attributes and methods on DedicatedWorkerGlobalScope.");
worker = startWorker("resources/global-interface-listing-worker.js.php");
worker.postMessage({ platformSpecific: true });
</script>
