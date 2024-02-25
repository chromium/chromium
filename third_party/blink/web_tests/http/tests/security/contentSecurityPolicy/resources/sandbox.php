<?php
if (isset($_GET["report-only"])) {
    header("Content-Security-Policy-Report-Only: sandbox " . $_GET["sandbox"]);
} else {
    header("Content-Security-Policy: sandbox " . $_GET["sandbox"]);
}
?>
<!DOCTYPE html>
<p>Ready</p>
<?php
if (isset($_GET["script-in-subframe"])) {
?>
<iframe src="data:text/html,<script>console.log('Script executed in child frame');</script>"></iframe>
<?php
} else {
?>
<script>
console.log("Script executed in iframe.");
window.secret = "I am a secret";
</script>
<?php
}
?>
