(function() {

if (!window.internals) {
    testFailed('Test must be run with --expose-internals-for-testing flag.');
    return;
}

// FIXME(https://crbug.com/43394):
// These properties should live in the element prototypes instead of on the individual instances.
function listElementProperties(type, createElement) {
    debug('[' + type.toUpperCase() + ' NAMESPACE ELEMENT PROPERTIES]');
    var namespace = internals[type + 'Namespace']();
    debug('namespace ' + namespace);
    var tags = internals[type + 'Tags']().filter(tag => !(createElement(tag) instanceof HTMLUnknownElement));
    var tagProperties = {};
    var commonProperties = null; // Will be a map containing the intersection of properties across all elements as keys.
    tags.forEach(function(tag) {
        var element = createElement(tag);
        // We don't read out the property descriptors here to avoid the test timing out.
        var properties = getAllPropertyNames(element);
        tagProperties[tag] = properties;
        if (commonProperties === null) {
            commonProperties = {};
            properties.forEach(function(property) {
                commonProperties[property] = true;
            });
        } else {
            var hasProperty = {};
            properties.forEach(function(property) {
                hasProperty[property] = true;
            });
            Object.getOwnPropertyNames(commonProperties).forEach(function(property) {
                if (!hasProperty[property])
                    delete commonProperties[property];
            });
        }
    });
    debug(escapeHTML('<common>'));
    Object.getOwnPropertyNames(commonProperties).sort().forEach(function(property) {
        debug('    property ' + property);
    });
    tags.forEach(function(tag) {
        debug(type + ' element ' + tag);
        tagProperties[tag].forEach(function(property) {
            if (!(property in commonProperties))
                debug('    property ' + property);
        });
    });
}

listElementProperties('html', function (tag) { return document.createElement(tag); });
listElementProperties('svg', function (tag) { return document.createElementNS('http://www.w3.org/2000/svg', tag); });

})();
