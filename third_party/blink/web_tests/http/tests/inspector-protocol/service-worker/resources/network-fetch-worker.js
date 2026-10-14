self.onmessage = (event) => {
  fetch(new Request(event.data.url, event.data.init)).then((response) => {
    return response.text().then((text) => event.source.postMessage(text));
  }, () => event.source.postMessage('FETCH_FAILED'));
};
