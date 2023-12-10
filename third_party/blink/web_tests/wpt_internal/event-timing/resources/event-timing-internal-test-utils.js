function mainThreadBusy(duration) {
  const now = performance.now();
  while (performance.now() < now + duration);
}

function addListeners(target, events) {
  const eventListener = (e) => {
    mainThreadBusy(200);
  };
  events.forEach(e => { target.addEventListener(e, eventListener); });
}

function observeUntilTargetEvent(observationSet, targetEvent) {
  return (entryList) => {
    for (const { name } of entryList.getEntries()) {
      if (observationSet.has(targetEvent)) {
        // ignore events after targetEvent
        return;
      }
      observationSet.add(name);
    }
  }
}

function filterAndAddToMap(events, map) {
  return function (entry) {
    if (events.includes(entry.name)) {
      map.set(entry.name, entry.interactionId);
      return true;
    }
    return false;
  }
}

async function createPerformanceObserverPromise(observeTypes, callback, readyToResolve
) {
  return new Promise(resolve => {
    new PerformanceObserver(entryList => {
      callback(entryList);

      if (readyToResolve()) {
        resolve();
      }
    }).observe({ entryTypes: observeTypes });
  });
}

async function interactAndObserve(interactionType, target, observerPromise) {
  let interactionPromise;
  switch (interactionType) {
    case 'menu-key': {
      addListeners(target,
        ['keydown', 'keyup', 'contextmenu']);
      eventSender.keyDown('ContextMenu');
      break;
    }
    case 'menu-keydown': {
      addListeners(target,
        ['keydown', 'contextmenu']);
      eventSender.keyDownOnly('ContextMenu');
      break;
    }
  }
  return observerPromise;
}
