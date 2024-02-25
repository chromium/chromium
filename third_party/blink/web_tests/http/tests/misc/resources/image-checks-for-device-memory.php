<?php
    $device_memory = $_SERVER["HTTP_DEVICE_MEMORY"] ?? null;

    if(isset($device_memory) && $device_memory != 0
    	 && ($device_memory & ($device_memory - 1)) == 0) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
