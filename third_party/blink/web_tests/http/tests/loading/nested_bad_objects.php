<?
if (isset($_GET['object'])) {
    header("Content-Type: nothing/bad-type");
    exit();
}
?>
<html>
    <script>
    if (window.testRunner)
        testRunner.dumpAsText();
    window.onload = function() {
        internals.updateLayoutAndRunPostLayoutTasks();
    };
    </script>
    <object type="image/jpeg" data="nested_bad_objects.php?object">
        <object type="image/jpeg" data="nested_bad_objects.php?object" />
    </object>
    PASS - nested image objects with bad mimetype do not cause a crash.
</html>
