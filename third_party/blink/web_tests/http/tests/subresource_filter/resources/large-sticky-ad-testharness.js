// |kLargeStickyAd| from web_feature.mojom.
const kLargeStickyAd = 3155;

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
  return new Promise((resolve) => requestAnimationFrame(() => { setTimeout(() => { resolve(); }) }));
}

function appendAdFrameTo(parent)  {
  let ad_frame = document.createElement('iframe');
  parent.appendChild(ad_frame);
  internals.setIsAdFrame(ad_frame.contentDocument);
}
