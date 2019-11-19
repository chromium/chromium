<?
header("Content-Type: application/signed-exchange;v=b3");
header("X-Content-Type-Options: nosniff");
header("Link: <https://127.0.0.1:8443/loading/sxg/resources/sxg-subresource-script.sxg>;rel=alternate;type=\"application/signed-exchange;v=b3\";anchor=\"https://127.0.0.1:8443/loading/sxg/resources/sxg-subresource-script.js\";");
echo(file_get_contents("sxg-subresource-origin-trial-page.sxg"));
?>
