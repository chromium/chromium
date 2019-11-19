<?php

# Generate token with the command:
# generate_token.py http://127.0.0.1:8000 UnsizedMediaPolicy --expire-timestamp=2000000000
header("Origin-Trial: Aihrm6c554aujVxiV9NcwvJnL4DuAWEIuHPcoByCCeIQB4Tm1Ok3RpftS2JosN4mvtIuQlYNaLGylvC/tndGdwYAAABaeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiVW5zaXplZE1lZGlhUG9saWN5IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
?>
<title>Unsized Media feature policy - enabled by origin trial</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
test(t => {
  assert_in_array('unsized-media', document.featurePolicy.features());
}, 'The unsized-media policy is available via origin trial');
</script>
