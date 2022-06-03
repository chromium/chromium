<?php

# Generate token with the command:
# generate_token.py http://127.0.0.1:8000 DocumentPolicyNegotiation --expire-timestamp=2000000000 --version=3
# generate_token.py http://127.0.0.1:8000 DocumentPolicy --expire-timestamp=2000000000 --version=3
header("Origin-Trial: Axn/kXM6qoOXmxBlMMHrogcwCs/FVzmvaE73xBkYLsPPpZyqbwTumJM4URDPqhJzAnMdbQ+eRJ4QYIHCGsNFxwkAAABheyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRG9jdW1lbnRQb2xpY3lOZWdvdGlhdGlvbiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==,AxnhgkI0WzZkEzNgegLy52O7KBsyvs3ktnqOEIj2S6i+HfAZPBoZSzW4RZpaA7ovyo3timDSCKy68msgEHhL8woAAABWeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRG9jdW1lbnRQb2xpY3kiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
?>
<title>DocumentPolicyNegotiation interface - enabled by origin trial</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<script>
test(t => {
  var iframeInterfaceNames = Object.getOwnPropertyNames(this.HTMLIFrameElement.prototype);
  assert_in_array('policy', iframeInterfaceNames);
}, 'Document Policy `policy` attribute exists in origin-trial enabled document.');
</script>
</body>
