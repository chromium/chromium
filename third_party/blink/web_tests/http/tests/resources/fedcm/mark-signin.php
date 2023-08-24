<?php
header("Idp-Signin-Status: action=signin");
header("Access-Control-Allow-Credentials: true");
header("Access-Control-Allow-Origin: " . $_SERVER["HTTP_ORIGIN"]);
?>
Header sent.
