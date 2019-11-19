(function() {
  "use strict";
  // Define functions one by one and do not override the whole
  // test_driver_internal as it masks the new testing fucntions
  // that will be added in the future.
  const leftButton = 0;

  function getInViewCenterPoint(rect) {
    var left = Math.max(0, rect.left);
    var right = Math.min(window.innerWidth, rect.right);
    var top = Math.max(0, rect.top);
    var bottom = Math.min(window.innerHeight, rect.bottom);

    var x = 0.5 * (left + right);
    var y = 0.5 * (top + bottom);

    return [x, y];
  }

  function getPointerInteractablePaintTree(element, frame) {
    var frameDocument = frame == window ? window.document : frame.contentDocument;
    if (!frameDocument.contains(element)) {
      return [];
    }

    var rectangles = element.getClientRects();
    if (rectangles.length === 0) {
      return [];
    }

    var centerPoint = getInViewCenterPoint(rectangles[0]);
    if ("elementsFromPoint" in document) {
      return frameDocument.elementsFromPoint(centerPoint[0], centerPoint[1]);
    } else if ("msElementsFromPoint" in document) {
      var rv = frameDocument.msElementsFromPoint(centerPoint[0], centerPoint[1]);
      return Array.prototype.slice.call(rv ? rv : []);
    } else {
      throw new Error("document.elementsFromPoint unsupported");
    }
  }

  function inView(element, frame) {
    var pointerInteractablePaintTree = getPointerInteractablePaintTree(element, frame);
    return pointerInteractablePaintTree.indexOf(element) !== -1 || element.contains(pointerInteractablePaintTree[0], frame);
  }

  function findElementInFrame(element, frame) {
    var foundFrame = frame;
    var frameDocument = frame == window ? window.document : frame.contentDocument;
    if (!frameDocument.contains(element)) {
      foundFrame = null;
      var frames = document.getElementsByTagName("iframe");
      for (let i = 0; i < frames.length; i++) {
        if (findElementInFrame(element, frames[i])) {
          foundFrame = frames[i];
          break;
        }
      }
    }
    return foundFrame;
  }

  window.test_driver_internal.click = function(element, coords) {
    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.pointerActionSequence(
            [{
              source: 'mouse',
              actions: [
              {name: 'pointerMove', x: coords.x, y: coords.y},
              {name: 'pointerDown', x: coords.x, y: coords.y, button: leftButton},
              {name: 'pointerUp', button: leftButton}
              ]
            }],
            resolve);
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  // https://w3c.github.io/webdriver/#element-send-keys
  window.test_driver_internal.send_keys = function(element, keys) {
    return new Promise((resolve, reject) => {
      element.focus();
      if (!window.eventSender)
        reject(new Error("No eventSender"));
      if (keys.length > 1)
        reject(new Error("No support for a sequence of multiple keys"));
      let eventSenderKeys = keys;
      let charCode = keys.charCodeAt(0);
      // See https://w3c.github.io/webdriver/#keyboard-actions and
      // EventSender::KeyDown().
      if (charCode == 0xE004) {
        eventSenderKeys = "Tab";
      } else if (charCode == 0xE050) {
        eventSenderKeys = "ShiftRight";
      } else if (charCode == 0xE012) {
        eventSenderKeys = "ArrowLeft";
      } else if (charCode == 0xE013) {
        eventSenderKeys = "ArrowUp";
      } else if (charCode == 0xE014) {
        eventSenderKeys = "ArrowRight";
      } else if (charCode == 0xE015) {
        eventSenderKeys = "ArrowDown";
      } else if (charCode >= 0xE000 && charCode <= 0xF8FF) {
        reject(new Error("No support for this code: U+" + charCode.toString(16)));
      }
      window.requestAnimationFrame(() => {
        window.eventSender.keyDown(eventSenderKeys);
        resolve();
      });
    });
  };

  window.test_driver_internal.freeze = function() {
    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.freeze();
        resolve();
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  window.test_driver_internal.action_sequence = function(actions) {
    if (window.top !== window) {
      return Promise.reject(new Error("can only send keys in top-level window"));
    }

    var didScrollIntoView = false;
    for (let i = 0; i < actions.length; i++) {
      var last_x_position = 0;
      var last_y_position = 0;
      var first_pointer_down = false;
      for (let j = 0; j < actions[i].actions.length; j++) {
        if ('origin' in actions[i].actions[j]) {
          if (typeof(actions[i].actions[j].origin) === 'string') {
             if (actions[i].actions[j].origin == "viewport") {
               last_x_position = actions[i].actions[j].x;
               last_y_position = actions[i].actions[j].y;
             } else if (actions[i].actions[j].origin == "pointer") {
               return Promise.reject(new Error("pointer origin is not supported right now"));
             } else {
               return Promise.reject(new Error("pointer origin is not given correctly"));
             }
          } else {
            var element = actions[i].actions[j].origin;
            var frame = findElementInFrame(element, window);
            if (frame == null) {
              return Promise.reject(new Error("element in different document or iframe"));
            }

            if (!inView(element, frame)) {
              if (didScrollIntoView)
                return Promise.reject(new Error("already scrolled into view, the element is not found"));

              element.scrollIntoView({behavior: "instant",
                                      block: "end",
                                      inline: "nearest"});
              didScrollIntoView = true;
            }

            var pointerInteractablePaintTree = getPointerInteractablePaintTree(element, frame);
            if (pointerInteractablePaintTree.length === 0 ||
                !element.contains(pointerInteractablePaintTree[0])) {
              return Promise.reject(new Error("element click intercepted error"));
            }

            var rect = element.getClientRects()[0];
            var centerPoint = getInViewCenterPoint(rect);
            last_x_position = actions[i].actions[j].x + centerPoint[0];
            last_y_position = actions[i].actions[j].y + centerPoint[1];
            if (frame != window) {
              var frameRect = frame.getClientRects();
              last_x_position += frameRect[0].left;
              last_y_position += frameRect[0].top;
            }
          }
        }

        if (actions[i].actions[j].type == "pointerDown" || actions[i].actions[j].type == "pointerMove") {
          actions[i].actions[j].x = last_x_position;
          actions[i].actions[j].y = last_y_position;
        }

        if ('parameters' in actions[i] && actions[i].parameters.pointerType == "touch") {
          if (actions[i].actions[j].type == "pointerMove" && !first_pointer_down) {
            actions[i].actions[j].type = "pause";
          } else if (actions[i].actions[j].type == "pointerDown") {
            first_pointer_down = true;
          } else if (actions[i].actions[j].type == "pointerUp") {
            first_pointer_down = false;
          }
        }
      }
    }

    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.pointerActionSequence(actions, resolve);
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  // Enable automation so we don't wait for user input on unimplemented APIs
  window.test_driver_internal.in_automation = true;

})();
