// |kLargeStickyAd| from web_feature.mojom.
const kLargeStickyAd = 3155;

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function forceLayoutUpdate() {
  return new Promise((resolve) => requestAnimationFrame(() => { setTimeout(() => { resolve(); }) }));
}

function appendAdFrameTo(parent)  {
  let ad_frame = document.createElement('iframe');
  parent.appendChild(ad_frame);
  internals.setIsAdFrame(ad_frame.contentDocument);
}
