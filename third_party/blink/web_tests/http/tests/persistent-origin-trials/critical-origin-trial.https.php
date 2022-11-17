<?php
// Token for FrobulatePersistent
$ORIGIN_TRIAL_TOKEN = "A7eQahvlWGVqTPZ/Rpyq3p+Lw+CZaKPs8POfJ7SURAykNb7kG6+xv4I3O4E03VALwnxZJy4aB83PX5q5yseoSQEAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6ODQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnQiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";
header("Origin-Trial: ". $ORIGIN_TRIAL_TOKEN);
header("Critical-Origin-Trial: FrobulatePersistent");
$headers = getallheaders();
$trials = $headers['X-Web-Test-Enabled-Origin-Trials'];
?>
<!DOCTYPE html>
<title>Test that the Critical-Origin-Trial header enables the request header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<script>
    test(function(){
        assert_equals("<?php echo($trials)?>", "FrobulatePersistent");
        this.add_cleanup(function() {
            window.open("support/cleanup.https.html");
        });
    }, "PersistentOriginTrial using Critical header");
</script>
