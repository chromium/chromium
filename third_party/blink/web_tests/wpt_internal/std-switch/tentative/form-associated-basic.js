const HTML_DOM = '/html/dom/';

function loadScript(src) {
  return new Promise((resolve, reject) => {
    let script = document.createElement('script');
    script.src = src;
    script.addEventListener('load', e => resolve(), {once: true});
    script.addEventListener('error', e => reject(), {once: true});
    document.body.appendChild(script);
  });
}

function formAssociatedTests(tag, reflectionDict) {
  test(() => {
    // TODO(tkent): Move the following checks to idlharness.js?
    let interface = document.createElement(tag).constructor;
    assert_not_own_property(interface, 'disabledFeatures');
    assert_not_own_property(interface, 'formAssociated');
    assert_not_own_property(interface, 'observedAttributes');
    assert_not_own_property(interface.prototype, 'adoptedCallback');
    assert_not_own_property(interface.prototype, 'attributeChangedCallback');
    assert_not_own_property(interface.prototype, 'connectedCallback');
    assert_not_own_property(interface.prototype, 'disconnectedCallback');
    assert_not_own_property(interface.prototype, 'formAssociatedCallback');
    assert_not_own_property(interface.prototype, 'formDisabledCallback');
    assert_not_own_property(interface.prototype, 'formResetCallback');
    assert_not_own_property(interface.prototype, 'formStateRestoreCallback');
  }, `Interface for ${tag} should not have custom-element-like properties`);

  test(() => {
    let control = document.createElement(tag);
    assert_array_equals(Object.getOwnPropertySymbols(control), []);
  }, `A ${tag} instance should not have Symbol properties`);

  test(() => {
    let control = document.createElement(tag);
    assert_equals(control.type, tag);
  }, `${tag} supports type property`);

  test(() => {
    let control = document.createElement(tag);
    assert_equals(control.form, null);

    let form1 = document.createElement('form');
    form1.appendChild(control);
    assert_equals(control.form, form1);

    let form2 = document.createElement('form');
    form2.id = 'connected-form';
    document.body.appendChild(form2);
    control.setAttribute('form', 'connected-form');
    control.remove();
    assert_equals(control.form, null);
    document.body.appendChild(control);
    assert_equals(control.form, document.getElementById('connected-form'));
  }, `${tag} supports form property`);

  test(() => {
    let control = document.createElement(tag);
    assert_true(control.willValidate);
    control.setAttribute('disabled', '');
    assert_false(control.willValidate);
    control.removeAttribute('disabled');
    let datalist = document.createElement('datalist');
    datalist.appendChild(control);
    assert_false(control.willValidate);
  }, `${tag} supports willValidate property`);

  test(() => {
    let control = document.createElement(tag);
    assert_true(control.validity.valid);
    assert_false(control.validity.customError);
    assert_equals(control.validationMessage, '');
    assert_true(control.checkValidity(), '1: ' + control);
    assert_true(control.reportValidity());

    control.setCustomValidity('Invalid!');
    assert_false(control.validity.valid);
    assert_true(control.validity.customError);
    assert_equals(control.validationMessage, 'Invalid!');
    assert_false(control.checkValidity(), '2: ' + control);
    assert_false(control.reportValidity());
  }, `${tag} supports form validation`);

  test(() => {
    let control = document.createElement(tag);
    assert_equals(control.labels.length, 0);

    let label = document.createElement('label');
    document.body.appendChild(label);
    label.appendChild(control);
    assert_array_equals(control.labels, [label], 'descendant association');
    assert_equals(label.control, control);

    document.body.appendChild(control);
    document.body.appendChild(label);
    assert_equals(control.labels.length, 0);
    control.id = 'control';
    label.htmlFor = 'control';
    assert_array_equals(control.labels, [label], 'for= association');
    assert_equals(label.control, control);
  }, `${tag} supports labels property`);

  promise_test(async () => {
    await loadScript(HTML_DOM + 'original-harness.js');
    await loadScript(HTML_DOM + 'new-harness.js');
    let targetElements = {};
    targetElements[tag] = {
      disabled: 'boolean',
      name: 'string',
    };
    for (let [key, value] of Object.entries(reflectionDict)) {
      targetElements[tag][key] = value;
    }
    mergeElements(targetElements);
    await loadScript(HTML_DOM + 'reflection.js');
  }, 'Setup reflection tests');
}
