<?php
// Token for FrobulatePersistent
$ORIGIN_TRIAL_TOKEN = "A7eQahvlWGVqTPZ/Rpyq3p+Lw+CZaKPs8POfJ7SURAykNb7kG6+xv4I3O4E03VALwnxZJy4aB83PX5q5yseoSQEAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6ODQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnQiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";
header("Origin-Trial: ". $ORIGIN_TRIAL_TOKEN);
$headers = getallheaders();
$trials = $headers['X-Web-Test-Enabled-Origin-Trials'];

$child = $_REQUEST['child'] == "true";

if (!$child) {
// Main page
?>
<!DOCTYPE html>
<title>Test that navigations in a new window enables persistent origin trials</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<script>
    test(
        () => {
            // Ensure that the test state is reset, by checking that the first
            // load does not have an enabled persistent trial.
            assert_equals("<?php echo($trials)?>", "");
        }, "TestPrimaryPageHasNoTrial");
    // Navigate to the same domain, and check that the trial header is set
    var child_window = window.open("window-origin-trial.https.php?child=true");
    fetch_tests_from_window(child_window);
  </script>
<?php
// End main page
} else {
// Child page
?>
<!DOCTYPE html>
<title>Test that navigations in a new window enables persistent origin trials</title>
<script src="/resources/testharness.js"></script>
<body>
<script>
    test(function() {
        // In the child, assert that the header is now applied without any
        // redirects
        assert_equals("<?php echo($trials)?>", "FrobulatePersistent");

        this.add_cleanup(function() {
            window.open("support/cleanup.https.html");
        });
    }, "PersistentInChild");
</script>
<?php
// End child page
}
