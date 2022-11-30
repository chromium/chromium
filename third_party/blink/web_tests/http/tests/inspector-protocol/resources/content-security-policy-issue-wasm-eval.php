<?php
header("Content-Security-Policy: script-src 'self' 'unsafe-inline';");
?>
<!DOCTYPE html>
<html>
  <body>
    <h2>Webpage with not allowed WebAssembly</h2>

    <script>
      const wasm_script = new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0]);
      WebAssembly.instantiate(wasm_script);
    </script>
  </body>
</html>