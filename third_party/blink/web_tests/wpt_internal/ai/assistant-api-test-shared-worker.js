self.importScripts("resources/utils.js");

self.onconnect = async e => {
  const port = e.ports[0];
  port.postMessage(await testPromptAPI());
};
