<!doctype html>
<html>
<body>
<?php
foreach (getallheaders() as $name => $value) {
    echo "$name: $value\n";
}
?>
</body>
</html>
