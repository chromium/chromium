function waitForWindowScrollEnd(end_x, end_y) {
  var last_changed_frame = 0;
  var last_x = scrollX;
  var last_y = scrollY;
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrames until 20 frames without observed changes or
      // until the window has scolled to the specified destination.
      if (frames - last_changed_frame > 20) {
        resolve();
      }
      if (!(isNaN(end_x) && isNaN(end_y))) {
        if ((isNaN(end_x) || scrollX == end_x) &&
            (isNaN(end_y) || scrollY == end_y)) {
          console.log(scrollY + "==" + end_y);
          resolve();
        }
      }
      if (window.scrollX != last_x || window.scrollY != last_y) {
        last_changed_frame = frames;
        last_x = scrollX;
        last_y = scrollY;
      }
      requestAnimationFrame(tick.bind(null, frames + 1));
    }
    tick(0);
  });
}