<?php
$override_arg = strtolower($_GET["apply"]);
if ($override_arg == 'none') {
  $override_arg = '';
}
?>

// Override (aka "monkey patch") builtin methods related to inserting new
// elements into a document. Used to replicate behaviour found in
// crbug.com/1193888, where overridden methods caused the OT validation to
// capture the wrong script origin for injected token validation.
// |overrides| determines which methods are overridden:
//  - Node.appendChild is always overridden
//  - 'all' will also override methods/overrides used to insert a <meta> tag
function overrideInsertionBuiltins(overrides) {

  function installPropertyChangeOverrides(element) {
    if (!element) {
      return;
    }
    const properties = [];
    if (element.tagName === 'META') {
      properties.push('httpEquiv');
    }
    if (properties.length) {
      overrideProperties(element, properties);
    }
  }

  function overrideProperties(element, properties) {
    const elementProto = Object.getPrototypeOf(element);

    properties.forEach((property) => {
      const builtinDescriptor = Object.getOwnPropertyDescriptor(
        elementProto, property);
      const overrideDescriptor = ({
        get: builtinDescriptor.get,
        set: (value) => {
          propertySetter(element, builtinDescriptor, value);
        },
        configurable: builtinDescriptor.configurable,
        enumerable: builtinDescriptor.enumerable,
      });
      Object.defineProperty(elementProto, property, overrideDescriptor);
    });
  }

  function propertySetter(element, builtinDescriptor, value) {
    builtinDescriptor.set.call(element, value);
  }

  const applyAll = (overrides === 'all');

  const builtin_appendChild = window.Node.prototype.appendChild;
  window.Node.prototype.appendChild = function () {
    if (applyAll) {
      installPropertyChangeOverrides(arguments[0]);
    }
    const result = builtin_appendChild.apply(this, arguments);
    return result;
  }
}

<?php if (!empty($override_arg)): ?>
overrideInsertionBuiltins('<?=$override_arg?>');
<?php endif ?>
