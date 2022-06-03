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
        test(() => {
          let feature_allowed;
          try {
            document.domain = document.domain;
            feature_allowed = true;
          } catch(e) {
            feature_allowed = false;
          }

          assert_false(feature_allowed, "Feature(Document Domain) should not be allowed by permissions policy.");
        });
      </script>
    </head>
    <body bgcolor="#ffffff">
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>