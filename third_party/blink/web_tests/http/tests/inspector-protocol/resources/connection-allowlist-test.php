<?php
  header("Content-Type: text/html");
  header("Access-Control-Allow-Origin: *");

  if (isset($_GET["enforced"])) {
    header("Connection-Allowlist: {$_GET["enforced"]}");
  }

  if (isset($_GET["report_only"])) {
    header("Connection-Allowlist-Report-Only: {$_GET["report_only"]}");
  }

  if (isset($_GET["allow_from"])) {
    header("Allow-Connection-Allowlist-From: {$_GET["allow_from"]}");
  }
?>
<!DOCTYPE html>
<title>Connection-Allowlist Test</title>
<p>Hello World</p>
<?php
  // Emits a nested frame, so that embedded-enforcement scenarios (which need a
  // requirement to already be in effect on the embedder) can be set up without
  // evaluating script inside this document.
  if (isset($_GET["child"])) {
    $child = htmlspecialchars($_GET["child"], ENT_QUOTES);
    $allowlist = isset($_GET["child_allowlist"])
        ? htmlspecialchars($_GET["child_allowlist"], ENT_QUOTES)
        : "";
    echo "<iframe connectionallowlist=\"{$allowlist}\" src=\"{$child}\"></iframe>\n";
  }
?>
