// This file provides a PermissionsHelper object which can be used by
// LayoutTests using testRunner to handle permissions. The methods in the object
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
      case "accessibility-events":
        return {name: "accessibility-events"};
      case "clipboard-read":
        return {name: "clipboard-read"};
      case "clipboard-write":
        return {name: "clipboard-write"};
      case "payment-handler":
        return {name: "payment-handler"};
      case "background-fetch":
        return {name: "background-fetch"};
      case "periodic-background-sync":
        return {name: "periodic-background-sync"};
      case "nfc":
        return {name: "nfc"};
      default:
        throw "Invalid permission name provided";
    }
  }

  return {
    setPermission: function(name, state) {
      return new Promise(function(resolver, reject) {
        navigator.permissions.query(nameToObject(name)).then(function(result) {
            if (result.state == state) {
                resolver()
                return;
            }

            result.onchange = function() {
                result.onchange = null;
                resolver();
            };

            testRunner.setPermission(name, state, location.origin, location.origin);
        });
      });
    }
  }
})();
