<?php
header("Permissions-Policy-Report-Only: geolocation=()");
header("Content-Type: application/xml");

echo '<?xml version="1.0"?>
<?xml-stylesheet href="resources/permissions-policy-report-only-in-xsl.xslt" type="text/xsl"?>
<page>
</page>';
?>