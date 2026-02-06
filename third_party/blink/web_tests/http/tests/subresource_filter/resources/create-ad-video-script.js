function createAdVideo() {
  const video = document.createElement('video');
  video.width = 100;
  video.height = 200;
  document.body.appendChild(video);

  // Wait for the highlighting to render before notifying done.
  requestAnimationFrame(function() {
    requestAnimationFrame(function() {
      if (window.testRunner)
        testRunner.notifyDone();
    });
  });
}

createAdVideo();
