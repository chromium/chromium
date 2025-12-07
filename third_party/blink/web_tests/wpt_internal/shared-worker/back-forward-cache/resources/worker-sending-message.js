let targetPort = null;

self.onconnect = (event) => {
  const port = event.source;

  port.onmessage = (e) => {
    const message = e.data;
    if (message === 'register') {
      targetPort = port;
      targetPort.postMessage('done');
    } else if (message.type === 'transfer') {
      targetPort = e.ports[0];
      port.postMessage('done');
    } else if (message === 'message') {
      if (targetPort) {
        targetPort.postMessage('evict');
        port.postMessage('done');
      } else {
        port.postMessage('error');
      }
    }
  };
  port.start();
};
