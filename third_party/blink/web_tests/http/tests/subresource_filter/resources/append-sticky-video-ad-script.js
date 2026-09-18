function appendStickyVideoAd() {
  const video = document.createElement('video');
  video.id = 'ad-video';
  video.src = '/resources/test.ogv';
  video.style.position = 'fixed';
  video.style.left = '0px';
  video.style.top = '0px';
  video.style.width = '100px';
  video.style.height = '100px';
  document.body.appendChild(video);
}

appendStickyVideoAd();
