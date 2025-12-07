// This file provides a PermissionsHelper object which can be used by
// LayoutTests to handle permissions. The methods in the object
// return promises so can be used to write idiomatic, race-free code.
//
// The current available methods are:
// - setPermission: given a permission name (known by testRunner) and a state,
// it will set the permission to the specified state and resolve the promise
// when done.
// Example:
// PermissionsHelper.setPermission('geolocation', 'prompt').then(runTest);
"use strict";

var PermissionsHelper = (function() {
  function nameToObject(permissionName) {
    switch (permissionName) {
      case "midi":
        return {name: "midi"};
      case "midi-sysex":
        return {name: "midi", sysex: true};
      case "push-messaging":
        return {name: "push", userVisibleOnly: true};
      case "notifications":
        return {name: "notifications"};
      case "geolocation":
        return {name: "geolocation"};
      case "background-sync":
        return {name: "background-sync"};
      case "clipboard-read-write":
        return {name: "clipboard-write", allowWithoutSanitization: true};
      case "clipboard-sanitized-write":
        return {name: "clipboard-write", allowWithoutSanitization: false};
      case "payment-handler":
        return {name: "payment-handler"};
      case "background-fetch":
        return {name: "background-fetch"};
      case "periodic-background-sync":
        return {name: "periodic-background-sync"};
      case "nfc":
        return {name: "nfc"};
      case "display-capture":
        return {name: "display-capture"};
      case "captured-surface-control":
          return {name: "captured-surface-control"};
      case "speaker-selection":
        return {name: "speaker-selection"};
      case "web-app-installation":
        return {name: "web-app-installation"};
      default:
        throw "Invalid permission name provided";
    }
  }

  return {
    setPermission: function(name, state) {
      return internals.setPermission(nameToObject(name), state, location.origin, location.origin);
    }
  }
})();
