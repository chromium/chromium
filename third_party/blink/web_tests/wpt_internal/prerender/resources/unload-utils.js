// Note: Following utility functions are expected to be used from
// unload-on-prerender-* test files.

function createTestUrl(nextState, uid) {
  const params = new URLSearchParams();
  params.set('state', nextState);
  params.set('uid', uid);
  return location.pathname + '?' + params.toString();
}

function createCrossOriginTestUrl(nextState, uid) {
  const path = createTestUrl(nextState, uid);
  const url = new URL(path, location.href);
  url.host = get_host_info().REMOTE_HOST;
  return url.href;
}

function openChannel(uid) {
  return new PrerenderChannel('prerender', uid);
}

function addFrame(url) {
  const frame = document.createElement('iframe');
  frame.src = url;
  document.body.appendChild(frame);
  return frame;
}

function addEventListeners(name, uid) {
  return new Promise(resolve => {
    const eventsSeen = [];
    ['pagehide', 'pageshow', 'visibilitychange'].forEach(eventName => {
      window.addEventListener(eventName, e => {
        eventsSeen.push(eventName + ' ' + name +
            (document.prerendering ? ' in prerendering' : ''));
        // The `unload` should be the last event.
        if (eventName === 'pagehide') {
          // Send the event logs in bulk (not per event) to avoid reordering.
          // PrerenderChannel#postMessage() doesn't guarantee the message order
          // as it internally uses fetch().
          sendChannelMessage(eventsSeen, uid);
          resolve();
        }
      });
    });
  });
}

function waitWindowMessage() {
  return new Promise(resolve => {
    window.addEventListener('message', e => resolve(e.data), { once: true });
  });
}

function waitChannelMessage(message, uid) {
  return new Promise(resolve => {
    const bc = openChannel(uid);
    bc._messages = [];
    bc.addEventListener('message', e => {
      const data = JSON.parse(e.data);
      bc._messages = bc._messages.concat(data);
      if (bc._messages[bc._messages.length - 1] === message) {
        bc.close();
        resolve(bc._messages);
      }
    });
  });
}

function sendChannelMessage(message, uid) {
  const bc = openChannel(uid);
  bc.postMessage(JSON.stringify(message));
  bc.close();
}
