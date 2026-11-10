// UseCounter feature values from web_feature.mojom.
const kLargeStickyAd = 3155;
const kOverlayPopupAd = 3253;
const kOverlayPopup = 3331;
const kStickyVideoAdDetected = 6023;
const kOverlayPrestitialAd = 6076;

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function waitForFirstContentfulPaint() {
  return new Promise(resolve => {
    if (performance.getEntriesByName('first-contentful-paint').length > 0) {
      resolve();
    } else {
      new PerformanceObserver((list, observer) => {
        if (list.getEntriesByName('first-contentful-paint').length > 0) {
          observer.disconnect();
          resolve();
        }
      }).observe({type: 'paint', buffered: true});
    }
  });
}

function forceLayoutUpdate() {
  return new Promise((resolve) =>
                         requestAnimationFrame(() => {setTimeout(() => {
                                                 resolve();
                                               })}));
}

// Hides or shows the page. `testRunner.setMainWindowHidden` takes effect
// asynchronously, so wait for the resulting visibility change.
function setMainWindowHidden(hidden) {
  return new Promise((resolve) => {
    document.addEventListener('visibilitychange', () => resolve(),
                              {once: true});
    testRunner.setMainWindowHidden(hidden);
  });
}

function appendAdFrameTo(parent) {
  let ad_frame = document.createElement('iframe');
  parent.appendChild(ad_frame);
  internals.setIsAdFrame(ad_frame.contentDocument);
  return ad_frame;
}
