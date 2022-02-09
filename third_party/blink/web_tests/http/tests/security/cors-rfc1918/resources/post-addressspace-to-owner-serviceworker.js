self.addEventListener('message', e => {
    e.ports[0].postMessage({
        "messageType": "AddressSpaceMessage",
        "origin": self.location.origin,
        "addressSpace": self.addressSpace
    });
});
