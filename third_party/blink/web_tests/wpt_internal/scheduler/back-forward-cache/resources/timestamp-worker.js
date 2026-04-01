const t = [];
let shouldSendTimestamps = false;

let task = () => {
  t.push(Date.now());
  if (shouldSendTimestamps) {
    self.postMessage(t);
    return;
  }
  scheduler.postTask(task, {delay: 50});
};
scheduler.postTask(task);

self.onmessage = msg => {
  if (msg.data === 'get_timestamps') {
    // Send the timestamps once we get one more, to ensure we have an entry
    // coming out of BFCache.
    shouldSendTimestamps = true;
  }
};
