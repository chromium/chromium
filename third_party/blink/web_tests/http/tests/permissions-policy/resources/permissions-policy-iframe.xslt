<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="page">
  <html>
    <head>
      <title> Test XSLT </title>
      <script>
        window.onload = () => {
          let message;
          try {
            document.domain = document.domain;
            message = 'document-domain allowed';
          } catch (_) {
            message = 'document-domain disallowed';
          }

          parent.postMessage(message, '*');
        };
      </script>
    </head>
    <body></body>
  </html>
</xsl:template>

</xsl:stylesheet>