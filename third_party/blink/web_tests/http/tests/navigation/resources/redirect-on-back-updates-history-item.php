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
        // This code inserts a new history item using pushState, and then it
        // replaces that history item with a navigation to a page that just
        // navigates us back to this page.  However, we set the "location"
        // cookie so that when we navigate back to this page, we actually
        // redirect to the value of the "location" cookie.

        setLocationCookie("redirect-updates-history-item-done.html");

        history.pushState(null, null, "");
        location.replace("redirect-on-back-updates-history-item-goback.html");
    }, 0);
}
</script>

<p>redirect-on-back-updates-history-item.php: You should not see this text!</p>
