<?php
header('cache-control: no-store');

$location=$_COOKIE['location'];
if ($location!="") {
    header('Status: 303 See Other');
    header('Location: '.$location);
    exit;
}
?>

<script src='redirect-updates-history-item.js'></script>
<script src="/resources/prevent-bfcache.js"></script>
<script>
onload = function() {
    setTimeout(function() {
        preventBFCache();
        // The first time through here (sessionStorage.done is false), this
        // code inserts a new history item using pushState, and then it
        // triggers a reload of the history item.  However, we set the
        // "location" cookie so that when we reload this page, we actually
        // redirect to the value of the "location" cookie.
        //
        // This loads the "goback" page, which bounces us back here after
        // setting sessionStorage.done to true.  The point of this test is to
        // ensure that going back actually performs a real navigation as
        // opposed to performing a "same document navigation" as would normally
        // be done when navigating back after a pushState.

        if (sessionStorage.done) {
            location.replace("redirect-updates-history-item-done.html"); 
        } else {
            history.pushState(null, null, "");

            setLocationCookie("redirect-on-reload-updates-history-item-goback.html");
            location.reload();
        }
    }, 0);
}
</script>

<p>redirect-on-reload-updates-history-item.php: You should not see this text!</p>
