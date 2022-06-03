// Avoid polluting the global scope.
(function(global_object) {

  // Save the list of property names of the global object before loading other scripts.
  var global_property_names = Object.getOwnPropertyNames(global_object);

  importScripts('/js-test-resources/global-interface-listing.js');

  self.addEventListener('message', function(event) {
    var globals = [];

    globalInterfaceListing(
        global_object, global_property_names, event.data.platformSpecific,
        string => globals.push(string));

    event.ports[0].postMessage({result: globals});
  });

})(this);
