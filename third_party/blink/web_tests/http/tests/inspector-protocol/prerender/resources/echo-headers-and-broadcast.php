<!doctype html>
<html>
<body>
<?php
foreach (getallheaders() as $name => $value) {
    echo "$name: $value\n";
}
?>
</body>
<script>
const bc = new BroadcastChannel('prerender');
bc.postMessage(document.body.textContent);
</script>
</html>
