<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="page">
  <html>
    <head>
      <title> Test XSLT </title>
      <script src="../../resources/testharness.js"></script>
      <script src="../../resources/testharnessreport.js"></script>
      <script>
        test(t => {
          navigator.geolocation.getCurrentPosition(
              t.step_func_done(),
              t.unreached_func(
                    "Feature(geolocation) should not be allowed by permissions policy.")
          );
        });
      </script>
    </head>
    <body bgcolor="#ffffff">
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>