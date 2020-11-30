self.addEventListener('message', (e) => {
  const port = e.data.port;
  port.onmessage = (e) => {
    const data = e.data;
    const transferList = [];
    if (data.rs) {
      transferList.push(data.rs);
    }
    port.postMessage(data, transferList);
  };
});
