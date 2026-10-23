<?php
$reportFile = fopen("csp-report." . $_GET["test"] . ".tmp", 'w');
$httpHeaders = $_SERVER;
ksort($httpHeaders, SORT_STRING);
foreach ($httpHeaders as $name => $value) {
    if ($name === "CONTENT_TYPE" || $name === "HTTP_REFERER" || $name === "REQUEST_METHOD") {
        fwrite($reportFile, "$name: $value\n");
    }
    if ($name === "HTTP_COOKIE" && !empty($_COOKIE["cspViolationReportCookie"])) {
        fwrite($reportFile, "Cookie: cspViolationReportCookie=" . $_COOKIE["cspViolationReportCookie"] . "\n");
    }
}

foreach ($_COOKIE as $name => $value)
    setcookie($name, "deleted", time() - 60, "/");

fwrite($reportFile, "=== POST DATA ===\n");
fwrite($reportFile, file_get_contents("php://input"));
fclose($reportFile);
rename("csp-report." . $_GET["test"] . ".tmp", "csp-report." . $_GET["test"] . ".txt");
?>
