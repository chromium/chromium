<?php
    // Prevent from being cached.
    header("Cache-Control: no-cache, private, max-age=0");
    header("Content-Type: text/html");
?>

<html>

<script>
  // This test consists of following 3 pages in its history.
  //   a. back-send-referrer.html
  //   b. back-send-referrer-helper.php
  //   c. back-send-referrer-helper.php?x=42
  // It expects to navigate a.->b.->c.-(back)->b. and the last navigation
  // handles the referrer correctly.
  window.addEventListener('pageshow', () => {
    document.getElementById('length').innerText = history.length;

    if (history.length == 2) {
      // Showing b. for the first time.
      // Navigate once more (in a timeout) to add a history entry.
      setTimeout(function() { document.loopback.submit(); }, 0);
    } else if (location.search) {
      // Showing c. The query means nothing.
      setTimeout(function() { history.back(); }, 0);
    } else {
      // Should be showing b. for the second time.
      if (window.testRunner) {
        testRunner.notifyDone();
      }
    }
  });
</script>

Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?><br>
history.length : <span id="length"></span>

<form name=loopback action="" method=GET>
  <input type="hidden" name="x" value="42">
</form>

</html>