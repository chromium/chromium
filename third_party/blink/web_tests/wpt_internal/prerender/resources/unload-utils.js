// Note: Following utility functions are expected to be used from
// unload-on-prerender-* test files.

function createTestUrl(nextState) {
  const params = new URLSearchParams();
  params.set('state', nextState);
  return location.pathname + '?' + params.toString();
}

function createCrossOriginTestUrl(nextState) {
  const path = createTestUrl(nextState);
  const url = new URL(path, location.href);
  url.host = get_host_info().REMOTE_HOST;
  return url.href;
}

function openChannel() {
  return new BroadcastChannel('prerender');
}

function addFrame(url) {
  const frame = document.createElement('iframe');
  frame.src = url;
  document.body.appendChild(frame);
  return frame;
}

function addEventListeners(name) {
  ['unload', 'pagehide', 'pageshow', 'visibilitychange'].forEach(eventName => {
    window.addEventListener(eventName, e => {
      const bc = openChannel();
      bc.postMessage(eventName + ' ' + name +
          (document.prerendering ? ' in prerendering' : ''));
      bc.close();
    });
  });
}

function waitWindowMessage() {
  return new Promise(resolve => {
    window.addEventListener('message', e => resolve(e.data), { once: true });
  });
}

function waitChannelMessage(message) {
  return new Promise(resolve => {
    const bc = openChannel();
    bc._messages = [];
    bc.addEventListener('message', e => {
      bc._messages.push(e.data);
      if (e.data == message) {
        bc.close();
        resolve(bc._messages);
      }
    });
  });
}
