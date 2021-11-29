<?PHP
//
// OPTIONS
//
if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS') {
    //
    // FAIL
    //
    if ($_GET['preflight'] == "fail-with-500") {
        header("HTTP/1.1 500");
        exit;
    }
    if ($_GET['preflight'] == "fail-without-allow") {
        header("HTTP/1.1 200");
        header("Access-Control-Allow-Origin: ${_SERVER['HTTP_ORIGIN']}");
        header("Access-Control-Allow-Methods: GET");
        exit;
    }

    //
    // PASS
    //
    if ($_GET['preflight'] == "pass") {
        header("HTTP/1.1 200");
        header("Access-Control-Allow-Origin: ${_SERVER['HTTP_ORIGIN']}");
        header("Access-Control-Allow-Methods: GET");
        header("Access-Control-Allow-Private-Network: true");
        exit;
    }
}

//
// GET
//
if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    header("HTTP/1.1 200");
    header("Access-Control-Allow-Origin: ${_SERVER['HTTP_ORIGIN']}");

    $arr = array('jsonpResult' => 'success',
                 'method' => $_SERVER['REQUEST_METHOD'],
                 'headers' => getallheaders());
    $result = json_encode($arr);

    if ($_GET['out'] == "img") {
        header('Content-Type: image/png');
        $fn = fopen("abe.png", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    } else if ($_GET['out'] == "frame") {
        echo "<script>window.top.postMessage(${result}, '*');</script>";
    } else {   
        header('Content-Type: application/json');
        echo $result;
    }
}

?>
