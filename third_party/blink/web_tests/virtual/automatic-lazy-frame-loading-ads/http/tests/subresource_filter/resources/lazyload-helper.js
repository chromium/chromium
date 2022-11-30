const waitUntilIdle = () => {
  return new Promise(resolve => {
    window.requestIdleCallback(() => resolve());
  });
};
const loaded_ids = [];
const appendAdFrame = (id, elm, loadingAttr) => {
  const frame = document.createElement('iframe');
  frame.width = 300;
  frame.height = 300;
  frame.id = id;
  if (loadingAttr) {
    frame.loading = loadingAttr;
  }

  elm.appendChild(frame);
  internals.setIsAdFrame(frame.contentDocument);

  // Simulate 3P domain
  const third_pary_origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  frame.src = `${third_pary_origin}/resources/dummy.html`;

  frame.onload = () => {
    loaded_ids.push(id)
  };
};
const isElementLoaded = (id) => loaded_ids.includes(id);
const waitForElementLoad = (id) => {
  return new Promise((resolve, reject) => {
    const elm = document.getElementById(id)
    if (isElementLoaded(id)) {
      resolve(true);
    } else {
      elm.addEventListener('load', () => {
        resolve(true);
      });
    }
  });
};
