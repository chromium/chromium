self.addEventListener('message', function(event) {
  switch (event.data.type) {
    case 'claim':
      self.clients.claim()
          .then(function(result) {
            if (result !== undefined) {
              event.data.port.postMessage(
                  'FAIL: claim() should be resolved with undefined');
              return;
            }
            event.data.port.postMessage('PASS');
          })
          .catch(function(error) {
            event.data.port.postMessage('FAIL: exception: ' + error.name);
          });
      break;
    case 'storeClients':
      self.clients.matchAll().then(function(result) {
        self.storedClients = result;
        event.data.port.postMessage('PASS');
      });
      break;
    case 'postMessageToStoredClients':
      for (let client of self.storedClients) {
        client.postMessage('dummyValue');
      }
      event.data.port.postMessage('PASS');
      break;
    case 'storePort':
      self.storedPort = event.ports[0];
      self.storedPort.postMessage('Port stored');
      break;
    case 'postMessageViaTransferredPort':
      if (self.storedPort) {
        self.storedPort.postMessage('Message from SW via transferred port');
        event.data.port.postMessage('PASS');
      } else {
        event.data.port.postMessage('FAIL: port not stored');
      }
      break;
  }
});
