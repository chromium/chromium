<?php
    header("ACCEPT-CH: DPR, Device-Memory, Width, Viewport-Width");
    header("ACCEPT-CH-Lifetime: 3600");
?>
<!DOCTYPE html>
<script src="../../../resources/testharness.js"></script>
<script src="../../../resources/testharnessreport.js"></script>
<script>
    var t = async_test("Verify that hints were sent on iframe subresources since parent opt-in.");
    window.addEventListener("message", t.step_func(function (message) {
        console.log(message.data);
        assert_equals(message.data, "success");
        t.done();
    }));
</script>
<iframe src="resources/iframe-accept-ch.html"></iframe>
