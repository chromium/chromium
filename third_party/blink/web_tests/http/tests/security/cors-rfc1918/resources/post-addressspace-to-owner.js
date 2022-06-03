if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  onmessage = () => {
      postMessage({
          "origin": self.location.origin,
          "addressSpace": self.addressSpace
      });
  };
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  onconnect = e => {
      const port = e.ports[0];
      port.onmessage = function () {
          port.postMessage({
              "origin": self.location.origin,
              "addressSpace": self.addressSpace
          });
      }
  };
}
