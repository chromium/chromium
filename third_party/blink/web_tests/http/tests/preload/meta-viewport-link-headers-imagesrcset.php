<?php
    header('Link: <http://127.0.0.1:8000/resources/square.png?large>; rel=preload; as=image; imagesrcset="http://127.0.0.1:8000/resources/square.png?small 300w, http://127.0.0.1:8000/resources/square.png?large 600w"; imagesizes="(min-width: 300px) and (max-width: 300px) 300px, 600px"', false);
    header('Link: <http://127.0.0.1:8000/resources/square.png?base>; rel=preload; as=image; imagesrcset="http://127.0.0.1:8000/resources/square.png?299 299w, http://127.0.0.1:8000/resources/square.png?300 300w, http://127.0.0.1:8000/resources/square.png?301 301w"; imagesizes="100vw"', false);
?>
<!DOCTYPE html>
<meta name="viewport" content="width=300">
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script>
    const numPreloads = 2;
    var t = async_test('Makes sure that Link headers support the imagesrcset and imagesizes attributes and respond to <meta content=viewport>');

    let loaded = [];
    function checkPreloads(perf) {
        for (let e of perf.getEntriesByType('resource')) {
            let q = e.name.indexOf("?");
            if (q >= 0) {
                loaded.push(e.name.substr(q + 1));
            }
        }
        if (loaded.length >= numPreloads) {
            assert_array_equals(loaded.sort(), ["300", "small"]);
            t.done();
        }
    }

    let observer = new PerformanceObserver(checkPreloads);
    observer.observe({entryTypes: ["resource"]});
    checkPreloads(performance);
</script>

