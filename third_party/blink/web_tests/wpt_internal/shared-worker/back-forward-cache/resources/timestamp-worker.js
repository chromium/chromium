const t = [];
setInterval(() => t.push(Date.now()), 50);

self.onconnect = e => {
  const port = e.ports[0];
  port.onmessage = (msg) => {
    if (msg.data === 'get_timestamps') {
      port.postMessage(t);
    }
  };
};