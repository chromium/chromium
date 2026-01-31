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
          var xhr = new XMLHttpRequest();
          xhr.open("GET", document.location.href, false);
        console.log(document.location.href);
          assert_throws_dom('NetworkError',
            () => xhr.send(),
          "Synchronous XHR.send should throw an exception when disabled");
        });
      </script>
    </head>
    <body bgcolor="#ffffff">
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>
