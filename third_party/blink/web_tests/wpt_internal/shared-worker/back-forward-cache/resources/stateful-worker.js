let worker_id = null;

// This worker sets a unique ID when first connected and posts that ID on when
// sent the "get_id" message.
onconnect = e => {
  const port = e.ports[0];
  if (worker_id === null) {
    worker_id = Math.random();
  }

  port.onmessage = event => {
    if (event.data === 'get_id') {
      port.postMessage({id: worker_id});
    }
  };
};
