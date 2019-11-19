<?php
header('X-ServiceWorker-ServerHeader: SetInTheServer');

$prefix = '';
// If PreflightTest is set:
// - Use PACAOrign, PACAHeaders, PACAMethods, PACACredentials, PACEHeaders,
//   PAuth, PAuthFail and PSetCookie* parameters in preflight.
// - Use $_GET['PreflightTest'] as HTTP status code.
// - Check Access-Control-Request-Method/Headers headers with
//   PACRMethod/Headers parameter, if set, in preflight.
//   The special value 'missing' for PACRHeaders can be used to
//   test for the absence of ACRHeaders on the preflight request.
if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS' && isset($_GET['PreflightTest'])) {
    $prefix = 'P';

    if (isset($_GET['PACRMethod']) &&
        $_GET['PACRMethod'] !=
        $_SERVER['HTTP_ACCESS_CONTROL_REQUEST_METHOD']) {
        header("HTTP/1.1 400");
        exit;
    }
    if (isset($_GET['PACRHeaders'])) {
       if ($_GET['PACRHeaders'] == 'missing') {
           if (isset($_SERVER['HTTP_ACCESS_CONTROL_REQUEST_HEADERS'])) {
               header("HTTP/1.1 400");
               exit;
           }
       } else if ($_GET['PACRHeaders'] !=
                  $_SERVER['HTTP_ACCESS_CONTROL_REQUEST_HEADERS']) {
           header("HTTP/1.1 400");
           exit;
       }
    }
    // Preflight must not include Cookie headers.
    if (isset($_SERVER['HTTP_COOKIE'])) {
        header("HTTP/1.1 400");
        exit;
    }
    header("HTTP/1.1 {$_GET['PreflightTest']}");
}

if (isset($_GET[$prefix . 'ACAOrigin'])) {
    $origins = explode(',', $_GET[$prefix . 'ACAOrigin']);
    for ($i = 0; $i < sizeof($origins); ++$i)
        header("Access-Control-Allow-Origin: " . $origins[$i], false);
}
if (isset($_GET[$prefix . 'ACAHeaders']))
    header('Access-Control-Allow-Headers: ' . $_GET[$prefix . 'ACAHeaders']);
if (isset($_GET[$prefix . 'ACAMethods']))
    header('Access-Control-Allow-Methods: ' . $_GET[$prefix . 'ACAMethods']);
if (isset($_GET[$prefix . 'ACACredentials']))
    header('Access-Control-Allow-Credentials: ' .
           $_GET[$prefix . 'ACACredentials']);
if (isset($_GET[$prefix . 'ACEHeaders']))
    header('Access-Control-Expose-Headers: ' . $_GET[$prefix . 'ACEHeaders']);

// Whether the Set-Cookie line needs to include SameSite=None and Secure (for
// cross-site use).
$cookie_cross_site = isset($_GET['SameSiteNone']);

if (isset($_GET[$prefix . 'SetCookie'])) {
    if ($cookie_cross_site) {
        header('Set-Cookie: cookie=' . $_GET[$prefix . 'SetCookie'] .
               '; SameSite=None; Secure');
    } else {
        header('Set-Cookie: cookie=' . $_GET[$prefix . 'SetCookie']);
    }
}
if (isset($_GET[$prefix . 'SetCookie2']))
    header('Set-Cookie2: cookie=' . $_GET[$prefix . 'SetCookie2']);

if ((isset($_GET[$prefix . 'Auth']) and !isset($_SERVER['PHP_AUTH_USER'])) ||
    isset($_GET[$prefix . 'AuthFail'])) {
    header('WWW-Authenticate: Basic realm="Restricted"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Authentication canceled';
    exit;
}

if (isset($_GET['PNGIMAGE'])) {
  header('Content-Type: image/png');
  echo base64_decode(
    'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAARnQU1B' .
    'AACxjwv8YQUAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAhSURBVDhPY3wro/KfgQLABKXJBqMG' .
    'jBoAAqMGDLwBDAwAEsoCTFWunmQAAAAASUVORK5CYII=');
  exit;
}

$username = 'undefined';
$password = 'undefined';
$cookie = 'undefined';
if (isset($_SERVER['PHP_AUTH_USER'])) {
    $username = $_SERVER['PHP_AUTH_USER'];
}
if (isset($_SERVER['PHP_AUTH_PW'])) {
    $password = $_SERVER['PHP_AUTH_PW'];
}
if (isset($_COOKIE['cookie'])) {
    $cookie = $_COOKIE['cookie'];
}

$files = array();
foreach ($_FILES as $key => $file) {
    $content = '';
    $fp = fopen($file['tmp_name'], 'r');
    if ($fp) {
        $content = $file['size'] > 0 ? fread($fp, $file['size']) : '';
        fclose($fp);
    }
    $files[] = array('key' => $key,
                     'name' => $file['name'],
                     'type' => $file['type'],
                     'error' => $file['error'],
                     'size' => $file['size'],
                     'content' => $content);
}

header('Content-Type: application/javascript');
$arr = array('jsonpResult' => 'success',
             'method' => $_SERVER['REQUEST_METHOD'],
             'headers' => getallheaders(),
             'body' => file_get_contents('php://input'),
             'files' => $files,
             'get' => $_GET,
             'post' => $_POST,
             'username' => $username,
             'password' => $password,
             'cookie' => $cookie);
$json = json_encode($arr);
echo "report( $json );";
?>
