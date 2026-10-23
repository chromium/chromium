<?php
require_once '../../../resources/portabilityLayer.php';

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Pragma: no-cache");

if (!sys_get_temp_dir()) {
    echo "FAIL: No temp dir was returned.\n";
    exit();
}

function setState($newState, $file)
{
    file_put_contents($file, $newState);
}

function getState($file)
{
    if (!file_exists($file)) {
        return "0";
    }
    return file_get_contents($file);
}

$stateFile = sys_get_temp_dir() . "/remember-bad-password-status";

$command = $_GET['command'] ?? null;
if ($command) {
    if ($command == "status")
        echo getState($stateFile);
    else if ($command == "reset")
        echo setState("0", $stateFile);
    exit();
}

if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_REQUEST['uid']) || ($_REQUEST['uid'] != $_SERVER['PHP_AUTH_USER'])) {
    header('WWW-Authenticate: Basic realm="WebKit Test Realm"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Authentication canceled';
        if (isset($_SERVER['PHP_AUTH_USER'])) {
        setState(getState($stateFile) + 1, $stateFile);
    }
    exit;
} else {
    echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
}
?>
