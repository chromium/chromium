(function() {
  "use strict";

  if (navigator.webdriver) {
    // Only add the bespoke automation below when running with `content_shell`
    // in protocol mode (i.e., not webdriver).
    return;
  }

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

  let keyPressFunc, keyDownFunc, keyUpFunc;
  const eventSender = window.eventSender;
  if (eventSender) {
    keyPressFunc = eventSender.keyDown.bind(eventSender);
    keyDownFunc = eventSender.keyDownOnly.bind(eventSender);
    keyUpFunc = eventSender.keyUp.bind(eventSender);
  }

  function sendKeysToEventSender(keys, func = keyPressFunc) {
    if (!func) {
      throw new Error("No eventSender");
    }
    for(var i = 0; i < keys.length; ++i) {
      const charCode = keys.charCodeAt(i);
      // See https://w3c.github.io/webdriver/#keyboard-actions and
      // EventSender::KeyDown().
      switch (charCode) {
        case 0xE003: func("Backspace"); break;
        case 0xE004: func("Tab"); break;
        case 0xE006:
        case 0xE007: func("Enter", "enter"); break;
        case 0xE008: func("ShiftLeft", "shiftKey"); break;
        case 0xE009: func("ControlLeft", "ctrlKey"); break;
        case 0xE00A: func("AltLeft", "altKey"); break;
        case 0xE00C: func("Escape"); break;
        case 0xE00D: func(" "); break;
        case 0xE00E: func("PageUp"); break;
        case 0xE00F: func("PageDown"); break;
        case 0xE010: func("End"); break;
        case 0xE011: func("Home"); break;
        case 0xE012: func("ArrowLeft"); break;
        case 0xE013: func("ArrowUp"); break;
        case 0xE014: func("ArrowRight"); break;
        case 0xE015: func("ArrowDown"); break;
        case 0xE016: func("Insert"); break;
        case 0xE017: func("Delete"); break;
        case 0xE03D: func("MetaLeft", "metaKey"); break;
        case 0xE050: func("ShiftRight"); break;
        default:
          if (charCode >= 0xE000 && charCode <= 0xF8FF) {
            throw new Error("No support for this code: U+" + charCode.toString(16));
          }
          func(keys[i]);
          break;
      }
    }
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
      if (element.localName === 'input' && element.type === 'file') {
          element.addEventListener('drop', resolve);
          eventSender.beginDragWithFiles([keys]);
          const centerX = element.offsetLeft + element.offsetWidth / 2;
          const centerY = element.offsetTop + element.offsetHeight / 2;
          // Moving the mouse could interfere with the test, if it also tries to control
          // mouse movements. This can cause differences between tests run with run_web_tests
          // and tests run with wptrunner.
          eventSender.mouseMoveTo(centerX * devicePixelRatio, centerY * devicePixelRatio);
          eventSender.mouseUp();
          return;
      }
      window.requestAnimationFrame(() => {
        try {
          sendKeysToEventSender(keys);
          resolve();
        } catch (e) {
          reject(e);
        }
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

  window.test_driver_internal.generate_test_report = function(message) {
    return new Promise(function(resolve, reject) {
      if (internals) {
        internals.generateTestReport(message);
        resolve();
      } else {
        reject(new Error("window.internals not enabled."));
      }
    });
  };

  window.test_driver_internal.action_sequence = function(actions) {
    if (window.top !== window) {
      return Promise.reject(new Error("can only send actions in top-level window"));
    }

    let hasKeyActions = false;
    let hasPointerActions = false;
    var didScrollIntoView = false;
    for (let i = 0; i < actions.length; i++) {
      var last_x_position = 0;
      var last_y_position = 0;
      var first_pointer_down = false;
      for (let j = 0; j < actions[i].actions.length; j++) {
        const action = actions[i].actions[j];
        const type = action.type;
        // TODO(crbug.com/893480): Currently, `gpuBenchmarking` handles pointer
        // actions, while `EventSender` handles key actions. Mixing both types
        // of actions in one action sequence is not supported.
        if (type == "keyDown" || type == "keyUp") {
          hasKeyActions = true;
          if (!hasPointerActions) {
            continue;
          }
        } else if (type != "pause") {
          // "pause" is supported in both types of actions.
          hasPointerActions = true
        }
        if (hasKeyActions && hasPointerActions) {
          return Promise.reject(new Error(
            "We do not support keydown and keyup mixed with other actions, " +
            "please use test_driver.send_keys. See crbug.com/893480."));
        }

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
              return Promise.reject(new Error("element event-dispatch intercepted error"));
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

        if (actions[i].actions[j].type == "pointerDown" ||
            actions[i].actions[j].type == "pointerMove" ||
            actions[i].actions[j].type == "scroll") {
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

    return new Promise(async function(resolve, reject) {
      if (hasKeyActions) {
        try {
          if (!keyDownFunc || !keyUpFunc) {
            throw new Error("No eventSender");
          }
          for (const innerActions of actions) {
            for (const action of innerActions.actions) {
              switch (action.type) {
                case "keyDown":
                  sendKeysToEventSender(action.value, keyDownFunc);
                  break;
                case "keyUp":
                  sendKeysToEventSender(action.value, keyUpFunc);
                  break;
                case "pause":
                  await new Promise((resolve) => setTimeout(resolve, action.duration));
                  break;
                default:
                  throw new Error(`Unexpected key action type: ${action.type}`);
              }
            }
          }
          resolve();
        } catch (e) {
          reject(e);
        }
      } else if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.pointerActionSequence(actions, resolve);
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  let virtualAuthenticatorManager_;

  async function findAuthenticator(authenticatorManager, authenticatorId) {
    let authenticators = (await authenticatorManager.getAuthenticators()).authenticators;
    let foundAuthenticator;
    for (let authenticator of authenticators) {
      if ((await authenticator.getUniqueId()).id == authenticatorId) {
        foundAuthenticator = authenticator;
        break;
      }
    }
    if (!foundAuthenticator) {
      throw "Cannot find authenticator with ID " + authenticatorId;
    }
    return foundAuthenticator;
  }

  async function loadVirtualAuthenticatorManager() {
    if (!virtualAuthenticatorManager_) {
      const {VirtualAuthenticatorManager} = await import(
          '/gen/third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.m.js');
      virtualAuthenticatorManager_ = VirtualAuthenticatorManager.getRemote();
    }
    return virtualAuthenticatorManager_;
  }

  function urlSafeBase64ToUint8Array(base64url) {
    let base64 = base64url.replace(/-/g, "+").replace(/_/g, "/");
    // Add padding to make the length of the base64 string divisible by 4.
    if (base64.length % 4 != 0)
      base64 += "=".repeat(4 - base64.length % 4);
    return Uint8Array.from(atob(base64), c => c.charCodeAt(0));
  }

  function uint8ArrayToUrlSafeBase64(array) {
    let binary = "";
    for (let i = 0; i < array.length; ++i)
      binary += String.fromCharCode(array[i]);

    return window.btoa(binary)
      .replace(/\+/g, "-")
      .replace(/\//g, "_")
      .replace(/=/g, "");
  }

  window.test_driver_internal.add_virtual_authenticator = async function(options) {
    let manager = await loadVirtualAuthenticatorManager();

    const {AuthenticatorAttachment, AuthenticatorTransport} = await import(
        '/gen/third_party/blink/public/mojom/webauthn/authenticator.mojom.m.js');
    const {ClientToAuthenticatorProtocol, Ctap2Version} = await import(
        '/gen/third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.m.js');

    options = Object.assign({
      hasResidentKey: false,
      hasUserVerification: false,
      isUserConsenting: true,
      isUserVerified: false,
      extensions: [],
    }, options);
    let mojoOptions = {};
    switch (options.protocol) {
      case "ctap1/u2f":
        mojoOptions.protocol = ClientToAuthenticatorProtocol.U2F;
        break;
      case "ctap2":
        mojoOptions.protocol = ClientToAuthenticatorProtocol.CTAP2;
        mojoOptions.ctap2Version = Ctap2Version.CTAP2_0;
        break;
      case "ctap2_1":
        mojoOptions.protocol = ClientToAuthenticatorProtocol.CTAP2;
        mojoOptions.ctap2Version = Ctap2Version.CTAP2_1;
        break;
      default:
        throw "Unknown protocol "  + options.protocol;
    }
    switch (options.transport) {
      case "usb":
        mojoOptions.transport = AuthenticatorTransport.USB;
        mojoOptions.attachment = AuthenticatorAttachment.CROSS_PLATFORM;
        break;
      case "nfc":
        mojoOptions.transport = AuthenticatorTransport.NFC;
        mojoOptions.attachment = AuthenticatorAttachment.CROSS_PLATFORM;
        break;
      case "ble":
        mojoOptions.transport = AuthenticatorTransport.BLE;
        mojoOptions.attachment = AuthenticatorAttachment.CROSS_PLATFORM;
        break;
      case "internal":
        mojoOptions.transport = AuthenticatorTransport.INTERNAL;
        mojoOptions.attachment = AuthenticatorAttachment.PLATFORM;
        break;
      default:
        throw "Unknown transport "  + options.transport;
    }
    mojoOptions.hasResidentKey = options.hasResidentKey;
    mojoOptions.hasUserVerification = options.hasUserVerification;
    mojoOptions.hasLargeBlob = options.extensions.indexOf("largeBlob") !== -1;
    mojoOptions.hasCredBlob = options.extensions.indexOf("credBlob") !== -1;
    mojoOptions.hasMinPinLength = options.extensions.indexOf("minPinLength") !== -1;
    mojoOptions.hasPrf = options.extensions.indexOf('prf') !== -1;
    mojoOptions.isUserPresent = options.isUserConsenting;

    let authenticator = (await manager.createAuthenticator(mojoOptions)).authenticator;
    await authenticator.setUserVerified(options.isUserVerified);
    return (await authenticator.getUniqueId()).id;
  };

  window.test_driver_internal.add_credential = async function(authenticatorId, credential) {
    if (credential.isResidentCredential) {
      throw "The mojo virtual authenticator manager does not support resident credentials";
    }
    let manager = await loadVirtualAuthenticatorManager();
    let authenticator = await findAuthenticator(manager, authenticatorId);

    let registration = {
      keyHandle: urlSafeBase64ToUint8Array(credential.credentialId),
      privateKey: urlSafeBase64ToUint8Array(credential.privateKey),
      rpId: credential.rpId,
      counter: credential.signCount,
    };
    let addRegistrationResponse = await authenticator.addRegistration(registration);
    if (!addRegistrationResponse.added) {
      throw "Could not add credential";
    }
  };

  window.test_driver_internal.get_credentials = async function(authenticatorId) {
    let manager = await loadVirtualAuthenticatorManager();
    let authenticator = await findAuthenticator(manager, authenticatorId);

    let getCredentialsResponse = await authenticator.getRegistrations();
    return getCredentialsResponse.keys.map(key => ({
      credentialId: uint8ArrayToUrlSafeBase64(key.keyHandle),
      privateKey: uint8ArrayToUrlSafeBase64(key.privateKey),
      rpId: key.rpId,
      signCount: key.counter,
      isResidentCredential: false,
    }));
  };

  window.test_driver_internal.remove_credential = async function(authenticatorId, credentialId) {
    let manager = await loadVirtualAuthenticatorManager();
    let authenticator = await findAuthenticator(manager, authenticatorId);

    let removeRegistrationResponse = await authenticator.removeRegistration(
        urlSafeBase64ToUint8Array(credentialId));
    if (!removeRegistrationResponse.removed) {
      throw "Could not remove credential";
    }
  };

  window.test_driver_internal.remove_all_credentials = async function(authenticatorId) {
    let manager = await loadVirtualAuthenticatorManager();
    let authenticator = await findAuthenticator(manager, authenticatorId);
    await authenticator.clearRegistrations();
  }

  window.test_driver_internal.set_user_verified = async function(authenticatorId, options) {
    let manager = await loadVirtualAuthenticatorManager();
    let authenticator = await findAuthenticator(manager, authenticatorId);
    await authenticator.setUserVerified(options.isUserVerified);
  }

  window.test_driver_internal.remove_virtual_authenticator = async function(authenticatorId) {
    let manager = await loadVirtualAuthenticatorManager();
    let response = await manager.removeAuthenticator(authenticatorId);
    if (!response.removed)
      throw "Could not remove authenticator";
  }

  window.test_driver_internal.set_permission = function(permission_params) {
    return internals.setPermission(permission_params.descriptor,
                                   permission_params.state);
  }

  window.test_driver_internal.set_storage_access = function(origin, embedding_origin, blocked) {
    return internals.setStorageAccess(origin, embedding_origin, blocked);
  }

  window.test_driver_internal.delete_all_cookies = function() {
    return internals.deleteAllCookies();
  }

  window.test_driver_internal.get_all_cookies = function() {
    return internals.getAllCookies();
  }

  window.test_driver_internal.get_named_cookie = function(name) {
    return internals.getNamedCookie(name);
  }

  window.test_driver_internal.get_computed_label = function(element) {
    return internals.getComputedLabel(element);
  }

  window.test_driver_internal.get_computed_role = function(element) {
    return internals.getComputedRole(element);
  }

  window.test_driver_internal.minimize_window = async () => {
    window.testRunner.setFrameWindowHidden(true);
    // Wait until the new state is reflected in the document
    while (!document.hidden) {
      await new Promise(resolve => setTimeout(resolve, 0));
    }
  };

  window.test_driver_internal.set_window_rect = async (rect, context) => {
    window.testRunner.setFrameWindowHidden(false);
    // Wait until the new state is reflected in the document
    while (document.hidden) {
      await new Promise(resolve => setTimeout(resolve, 0));
    }
    if (rect !== undefined)
        window.testRunner.setWindowRect(rect);
  };

  window.test_driver_internal.get_window_rect = async function() {
      return {'x': window.screenX, 'y': window.screenY, 'width': window.outerWidth, 'height': window.outerHeight};
  }

  window.test_driver_internal.set_rph_registration_mode = async function (mode, context) {
      window.testRunner.setRphRegistrationMode(mode);
  };

  window.test_driver_internal.get_fedcm_dialog_type = async function() {
    return internals.getFedCmDialogType();
  }

  window.test_driver_internal.get_fedcm_dialog_title = async function() {
    // TODO(crbug.com/331237005): Return a subtitle, if we have one.
    return {title: await internals.getFedCmTitle()};
  }

  window.test_driver_internal.select_fedcm_account = async function(account_index) {
    return internals.selectFedCmAccount(account_index);
  }

  window.test_driver_internal.cancel_fedcm_dialog = async function() {
    return internals.dismissFedCmDialog();
  }

  window.test_driver_internal.click_fedcm_dialog_button = async function(dialog_button) {
    return internals.clickFedCmDialogButton(dialog_button);
  }

  window.test_driver_internal.create_virtual_sensor = function(
      sensor_type, sensor_params) {
    return internals.createVirtualSensor(sensor_type, sensor_params);
  }

  window.test_driver_internal.update_virtual_sensor = function(
      sensor_type, reading) {
    return internals.updateVirtualSensor(sensor_type, reading);
  }

  window.test_driver_internal.remove_virtual_sensor = function(sensor_type) {
    return internals.removeVirtualSensor(sensor_type);
  }

  window.test_driver_internal.get_virtual_sensor_information = function(
      sensor_type) {
    return internals.getVirtualSensorInformation(sensor_type);
  }

  window.test_driver_internal.set_device_posture = function(posture) {
    return internals.setDevicePostureOverride(posture);
  }

  window.test_driver_internal.clear_device_posture = function() {
    return internals.clearDevicePostureOverride();
  }

  window.test_driver_internal.create_virtual_pressure_source = function(
      source_type, metadata) {
    return internals.createVirtualPressureSource(source_type, metadata);
  }

  window.test_driver_internal.update_virtual_pressure_source = function(
      source_type, sample) {
    return internals.updateVirtualPressureSource(source_type, sample);
  }

  window.test_driver_internal.remove_virtual_pressure_source = function(
      source_type) {
    return internals.removeVirtualPressureSource(source_type);
  }

  // Enable automation so we don't wait for user input on unimplemented APIs
  window.test_driver_internal.in_automation = true;

})();
