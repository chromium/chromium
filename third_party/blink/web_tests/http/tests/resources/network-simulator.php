<?php
require_once 'portabilityLayer.php';

// This script acts as a stateful proxy for retrieving files. When the state is set to
// offline, it simulates a network error with a nonsense response.

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
        return "Uninitialized";
    }
    return file_get_contents($file);
}

function contentType($path)
{
    if (preg_match("/\.html$/", $path))
        return "text/html";
    if (preg_match("/\.manifest$/", $path))
        return "text/cache-manifest";
    if (preg_match("/\.js$/", $path))
        return "text/javascript";
    if (preg_match("/\.xml$/", $path))
        return "application/xml";
    if (preg_match("/\.xhtml$/", $path))
        return "application/xhtml+xml";
    if (preg_match("/\.svg$/", $path))
        return "application/svg+xml";
    if (preg_match("/\.xsl$/", $path))
        return "application/xslt+xml";
    if (preg_match("/\.gif$/", $path))
        return "image/gif";
    if (preg_match("/\.jpg$/", $path))
        return "image/jpeg";
    if (preg_match("/\.png$/", $path))
        return "image/png";
    return "text/plain";
}

function generateNoCacheHTTPHeader()
{
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-cache, no-store, must-revalidate");
    header("Pragma: no-cache");
}

function generateResponse($path)
{
    global $stateFile;
    $state = getState($stateFile);
    if ($state == "Offline") {
        # Simulate a network error by replying with a nonsense response.
        header('HTTP/1.1 307 Temporary Redirect');
        header('Location: ' . $_SERVER['REQUEST_URI']); # Redirect to self.
        header('Content-Length: 1');
        header('Content-Length: 5', false); # Multiple content-length headers, some network stacks can detect this condition faster.
        echo "Intentionally incorrect response.";
    } else {
        // A little securuty checking can't hurt.
        if (strstr($path, ".."))
            exit;

        if ($path[0] == '/')
            $path = '..' . $path;

        if (!$_GET['allow-caching'])
            generateNoCacheHTTPHeader();

        if (file_exists($path)) {
            header("Last-Modified: " . gmdate("D, d M Y H:i:s T", filemtime($path)));
            header("Content-Type: " . contentType($path));

            print file_get_contents($path);
        } else {
            header('HTTP/1.1 404 Not Found');
        }
    }
}

function handleIncreaseResourceCountCommand($path)
{
    $resourceCountFile = sys_get_temp_dir() . "/resource-count";
    $resourceCount = getState($resourceCountFile);
    $pieces = explode(" ", $resourceCount);
    $count = 0;
    if (count($pieces) == 2 && $pieces[0] == $path) {
        $count = 1 + $pieces[1];
    } else {
        $count = 1;
    }
    file_put_contents($resourceCountFile, $path . " " . $count);
    generateResponse($path);
}

function handleResetResourceCountCommand()
{
    $resourceCountFile = sys_get_temp_dir() . "/resource-count";
    file_put_contents($resourceCountFile, 0);
    generateNoCacheHTTPHeader();
    header('HTTP/1.1 200 OK');
}

function handleGetResourceCountCommand($path)
{
    $resourceCountFile = sys_get_temp_dir() . "/resource-count";
    $resourceCount = getState($resourceCountFile);
    $pieces = explode(" ", $resourceCount);
    generateNoCacheHTTPHeader();
    header('HTTP/1.1 200 OK');
    if (count($pieces) == 2 && $pieces[0] == $path) {
        echo $pieces[1];
    } else {
        echo 0;
    }
}

# Do not use for new tests as this logging functionalities are not maintained,
# and are not safe to run tests in parallel. Accesses for other tests may be
# merged, or other commands may trim the log.
function handleStartResourceRequestsLog()
{
    $resourceLogFile = sys_get_temp_dir() . "/resource-log";
    file_put_contents($resourceLogFile,  "");
}

function handleClearResourceRequestsLog()
{
    $resourceLogFile = sys_get_temp_dir() . "/resource-log";
    file_put_contents($resourceLogFile, "");
}

function handleGetResourceRequestsLog()
{
    $resourceLogFile = sys_get_temp_dir() . "/resource-log";

    generateNoCacheHTTPHeader();
    header("Content-Type: text/plain");

    print file_get_contents($resourceLogFile);
}

function handleLogResourceRequest($path)
{
    $resourceLogFile = sys_get_temp_dir() . "/resource-log";

    $newData = "\n".$path;
    file_put_contents($resourceLogFile, $newData, FILE_APPEND | LOCK_EX);
}

$stateFile = sys_get_temp_dir() . "/network-simulator-state";
$command = $_GET['command'];
if ($command) {
    if ($command == "connect")
        setState("Online", $stateFile);
    else if ($command == "disconnect")
        setState("Offline", $stateFile);
    else if ($command == "increase-resource-count")
        handleIncreaseResourceCountCommand($_GET['path']);
    else if ($command == "reset-resource-count")
        handleResetResourceCountCommand();
    else if ($command == "get-resource-count")
        handleGetResourceCountCommand($_GET['path']);
    else if ($command == "start-resource-request-log")
        handleStartResourceRequestsLog();
    else if ($command == "clear-resource-request-log")
        handleClearResourceRequestsLog();
    else if ($command == "get-resource-request-log")
        handleGetResourceRequestsLog();
    else if ($command == "log-resource-request")
        handleLogResourceRequest($_GET['path']);
    else
        echo "Unknown command: " . $command . "\n";
    exit();
}

$requestedPath = $_GET['path'];
generateResponse($requestedPath);
?>
