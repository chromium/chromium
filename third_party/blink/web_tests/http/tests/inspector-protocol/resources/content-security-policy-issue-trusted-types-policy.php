<?php
if(isset($_GET['rpt'])) {
  header("Content-Security-Policy-Report-Only: require-trusted-types-for 'script'; trusted-types policy1");
} else {
  header("Content-Security-Policy: require-trusted-types-for 'script'; trusted-types policy1");
}
?>
<!DOCTYPE html>
<html>
  <body>
    <script>
      const policy2 = trustedTypes.createPolicy("policy2", {
        createHTML: string => string,
      });
    </script>
  </body>
</html>
