<?php
  if (isset($_GET["csp"]))
    header("Content-Security-Policy: treat-as-public-address");
?><script>
    window.parent.postMessage({
        "origin": window.location.origin,
        "addressSpace": document.addressSpace
    }, "*");
</script>
