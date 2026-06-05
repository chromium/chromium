function queryPlayerStatus(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === "queryPlayerStatus") {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'queryPlayerStatus'}, '*');
  });
}

function playMediaInIframe(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === "play") {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'play'}, '*');
  });
}

function pauseMediaInIframe(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === "pause") {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'pause'}, '*');
  });
}

function expectMediaPlayerStateChangeInIframe() {
  return new Promise(resolve => {
    function handler(event) {
      if (event.data.type === "statechange") {
        window.removeEventListener('message', handler);
        resolve(event.data.newState);
      }
    }

    window.addEventListener('message', handler);
    setTimeout(() => {
      window.removeEventListener('message', handler);
      resolve('Timed out waiting for statechange');
    }, 2000);
  });
}