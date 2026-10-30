<?php
header("Content-Type: text/css");
if (isset($_GET["header"])) {
  header("SourceMap: " . $_GET["header"]);
}
if (isset($_GET["xheader"])) {
  header("X-SourceMap: " . $_GET["xheader"]);
}
?>
body { color: red; }
<?php
if (isset($_GET["comment"])) {
  echo "/*# sourceMappingURL=" . $_GET["comment"] . " */\n";
}
?>
