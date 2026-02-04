'use strict'

// Returns a promise that resolves when a |name| event is fired.
function waitUntilEvent(obj, name) {
  return new Promise(r => obj.addEventListener(name, r, {once: true}));
}

// Returns a promise that resolves when |pc.connectionState| is in one of the
// wanted states.
async function waitForConnectionStateChange(pc, wantedStates) {
  while (!wantedStates.includes(pc.connectionState)) {
    await waitUntilEvent(pc, 'connectionstatechange');
  }
}

async function waitForIceGatheringState(pc, wantedStates) {
  while (!wantedStates.includes(pc.iceGatheringState)) {
    await waitUntilEvent(pc, 'icegatheringstatechange');
  }
}
