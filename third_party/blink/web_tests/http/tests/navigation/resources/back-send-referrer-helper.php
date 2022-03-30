<?php
    // Prevent from being cached.
    header("Cache-Control: no-cache, private, max-age=0");
    header("Content-Type: text/html");
?>

<html>

Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?><br>
sessionStorage.name : <span id="name"></span>

<form name=loopback action="" method=GET></form>

<script>
  // This test consists of following 3 pages in its history.
  //   a. back-send-referrer.html
  //   b. back-send-referrer-helper.php
  //   c. back-send-referrer-helper.php?
  // It expects to navigate a.->b.->c.-(back)->b. and the last navigation
  // handles the referrer correctly.
  window.addEventListener('pageshow', () => {
    sessionStorage.setItem("name", parseInt(sessionStorage.getItem("name")) + 1);
    document.getElementById("name").innerText = sessionStorage.getItem("name");

    setTimeout(function() {
      const name = sessionStorage.getItem("name");
      if (name == 1) {
        // Showing b. for the first time.
        // Navigate once more (in a timeout) to add a history entry.
        document.loopback.submit();
      } else if (name == 2) {
        // Showing c. The query means nothing.
        history.back();
      } else {
        // Should be showing b. for the second time.
        // TODO(https://crbug.com/1311546): Run this with a delay to work around a race when
        // BFCache is enabled.
        setTimeout(function() {
          if (window.testRunner)
            testRunner.notifyDone();
        }, 500);
      }
    }, 0);
  });
</script>

</html>