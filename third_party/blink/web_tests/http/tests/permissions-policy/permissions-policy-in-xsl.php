<?php
header("Permissions-Policy: geolocation=()");
header("Content-Type: application/xml");

echo '<?xml version="1.0"?>
<?xml-stylesheet href="resources/permissions-policy-in-xsl.xslt" type="text/xsl"?>
<page>
</page>';
?>
