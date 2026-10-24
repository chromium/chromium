<?php
$refer = $_SERVER['HTTP_REFERER'] ?? null;
if ($refer && stristr($refer, "file:") == 0)
    header('Location: http://127.0.0.1:8000/security/resources/red200x100.png');
else 
    header('Location: http://127.0.0.1:8000/security/resources/green250x50.png');
?>
