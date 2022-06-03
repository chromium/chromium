self.addEventListener('message', function(e) {
    const message = e.data;
    if ('port' in message) {
      const port = message.port;
      self.addEventListener('timezonechange', function(evt) {
        port.postMessage('SUCCESS');
      });
      port.postMessage('READY');
    }
});
