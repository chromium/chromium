/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.formsTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dom = goog.require('goog.dom');
const forms = goog.require('goog.dom.forms');
const testSuite = goog.require('goog.testing.testSuite');

const stubs = new PropertyReplacer();

/**
 * Sets up a mocked version of goog.window.openBlank.
 * @param {!Object} mockForm A mocked form Object to return on
 *     createElement('form').
 */
function mockWindowOpen(mockForm) {
  const windowOpen = () => ({
    document: {
      createElement: function(name) {
        if (name == 'form') {
          return mockForm;
        }
        return {};
      },
    },
  });
  stubs.setPath('goog.window.openBlank', windowOpen);
}

testSuite({
  tearDown() {
    stubs.reset();
  },

  testSubmitFormInNewWindowWithSubmitButton() {
    const expectedForm = [
      {name: 'in1', value: 'foo', type: 'hidden'},
      {name: 'in2', value: 'bar', type: 'hidden'},
      {name: 'in2', value: 'baaz', type: 'hidden'},
      {name: 'in3', value: '', type: 'hidden'},
      {name: 'pass', value: 'bar', type: 'hidden'},
      {name: 'textarea', value: 'foo bar baz', type: 'hidden'},
      {name: 'select1', value: '1', type: 'hidden'},
      {name: 'select2', value: 'a', type: 'hidden'},
      {name: 'select2', value: 'c', type: 'hidden'},
      {name: 'select3', value: '', type: 'hidden'},
      {name: 'checkbox1', value: 'on', type: 'hidden'},
      {name: 'radio', value: 'X', type: 'hidden'},
      {name: 'radio2', value: 'Y', type: 'hidden'},
      {name: 'submit', value: 'submitb', type: 'hidden'},
    ];

    const formElements = [];
    const mockForm = {};

    const appendChild = HTMLFormElement.prototype.appendChild;
    const submit = HTMLFormElement.prototype.submit;

    /**
     * @suppress {missingReturn} suppression added to enable type checking
     */
    HTMLFormElement.prototype.appendChild = (child) => {
      formElements.push(child);
    };
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    HTMLFormElement.prototype.submit = () => {
      assertArrayEquals(expectedForm, formElements);
      assertEquals('https://foo.xyz/baz', mockForm.action);
      assertEquals('get', mockForm.method);
    };
    mockWindowOpen(mockForm);

    const formEl = dom.getElement('testform1');
    const submitEl = dom.getElement('submitb');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.submitFormInNewWindow(formEl, submitEl);
    assertTrue(result);
    HTMLFormElement.prototype.appendChild = appendChild;
    HTMLFormElement.prototype.submit = submit;
  },

  testSubmitFormInNewWindowWithSubmitInput() {
    const expectedForm = [
      {name: 'in1', value: 'foo', type: 'hidden'},
      {name: 'in2', value: 'bar', type: 'hidden'},
      {name: 'in2', value: 'baaz', type: 'hidden'},
      {name: 'in3', value: '', type: 'hidden'},
      {name: 'pass', value: 'bar', type: 'hidden'},
      {name: 'textarea', value: 'foo bar baz', type: 'hidden'},
      {name: 'select1', value: '1', type: 'hidden'},
      {name: 'select2', value: 'a', type: 'hidden'},
      {name: 'select2', value: 'c', type: 'hidden'},
      {name: 'select3', value: '', type: 'hidden'},
      {name: 'checkbox1', value: 'on', type: 'hidden'},
      {name: 'radio', value: 'X', type: 'hidden'},
      {name: 'radio2', value: 'Y', type: 'hidden'},
      {name: 'submit', value: 'submitv', type: 'hidden'},
    ];

    const formElements = [];
    const mockForm = {};

    const appendChild = HTMLFormElement.prototype.appendChild;
    const submit = HTMLFormElement.prototype.submit;
    /**
     * @suppress {missingReturn} suppression added to enable type checking
     */
    HTMLFormElement.prototype.appendChild = (child) => {
      formElements.push(child);
    };
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    HTMLFormElement.prototype.submit = () => {
      assertArrayEquals(expectedForm, formElements);
      assertEquals('https://foo.xyz/baz', mockForm.action);
      assertEquals('get', mockForm.method);
    };
    mockWindowOpen(mockForm);

    const formEl = dom.getElement('testform1');
    const submitEl = dom.getElement('submit');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.submitFormInNewWindow(formEl, submitEl);
    assertTrue(result);
    HTMLFormElement.prototype.appendChild = appendChild;
    HTMLFormElement.prototype.submit = submit;
  },

  testSubmitFormInNewWindowWithoutSubmitButton() {
    const expectedForm = [
      {name: 'in1', value: 'foo', type: 'hidden'},
      {name: 'in2', value: 'bar', type: 'hidden'},
      {name: 'in2', value: 'baaz', type: 'hidden'},
      {name: 'in3', value: '', type: 'hidden'},
      {name: 'pass', value: 'bar', type: 'hidden'},
      {name: 'textarea', value: 'foo bar baz', type: 'hidden'},
      {name: 'select1', value: '1', type: 'hidden'},
      {name: 'select2', value: 'a', type: 'hidden'},
      {name: 'select2', value: 'c', type: 'hidden'},
      {name: 'select3', value: '', type: 'hidden'},
      {name: 'checkbox1', value: 'on', type: 'hidden'},
      {name: 'radio', value: 'X', type: 'hidden'},
      {name: 'radio2', value: 'Y', type: 'hidden'},
    ];

    const formElements = [];
    const mockForm = {};

    const appendChild = HTMLFormElement.prototype.appendChild;
    const submit = HTMLFormElement.prototype.submit;

    /**
     * @suppress {missingReturn} suppression added to enable type checking
     */
    HTMLFormElement.prototype.appendChild = (child) => {
      formElements.push(child);
    };
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    HTMLFormElement.prototype.submit = () => {
      assertArrayEquals(expectedForm, formElements);
      assertEquals('https://foo.bar/baz', mockForm.action);
      assertEquals('get', mockForm.method);
    };
    mockWindowOpen(mockForm);

    const formEl = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.submitFormInNewWindow(formEl);
    assertTrue(result);
    HTMLFormElement.prototype.appendChild = appendChild;
    HTMLFormElement.prototype.submit = submit;
  },

  testSubmitFormInNewWindowError() {
    const formEl = dom.getElement('testform1');
    const resetEl = dom.getElement('reset');

    assertThrows(
        'Non-submit type elements cannot be used to submit form.', /**
                                                                      @suppress {checkTypes}
                                                                      suppression
                                                                      added to
                                                                      enable
                                                                      type
                                                                      checking
                                                                    */
        () => {
          forms.submitFormInNewWindow(formEl, resetEl);
        });
  },

  testSubmitFormDataInNewWindow() {
    const expectedForm = [
      {name: 'in1', value: 'foo', type: 'hidden'},
      {name: 'in2', value: 'bar', type: 'hidden'},
      {name: 'in2', value: 'baaz', type: 'hidden'},
      {name: 'in3', value: '', type: 'hidden'},
      {name: 'pass', value: 'bar', type: 'hidden'},
      {name: 'textarea', value: 'foo bar baz', type: 'hidden'},
      {name: 'select1', value: '1', type: 'hidden'},
      {name: 'select2', value: 'a', type: 'hidden'},
      {name: 'select2', value: 'c', type: 'hidden'},
      {name: 'select3', value: '', type: 'hidden'},
      {name: 'checkbox1', value: 'on', type: 'hidden'},
      {name: 'radio', value: 'X', type: 'hidden'},
      {name: 'radio2', value: 'Y', type: 'hidden'},
    ];

    const formElements = [];
    const mockForm = {};

    const appendChild = HTMLFormElement.prototype.appendChild;
    const submit = HTMLFormElement.prototype.submit;
    /**
     * @suppress {missingReturn} suppression added to enable type checking
     */
    HTMLFormElement.prototype.appendChild = (child) => {
      formElements.push(child);
    };
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    HTMLFormElement.prototype.submit = () => {
      assertArrayEquals(expectedForm, formElements);
      assertEquals('https://foo.bar/baz', mockForm.action);
      assertEquals('get', mockForm.method);
    };
    mockWindowOpen(mockForm);

    const formEl = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const formData = forms.getFormDataMap(formEl);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const result =
        forms.submitFormDataInNewWindow(formEl.action, formEl.method, formData);
    assertTrue(result);
    HTMLFormElement.prototype.appendChild = appendChild;
    HTMLFormElement.prototype.submit = submit;
  },

  testGetFormDataString() {
    const el = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getFormDataString(el);
    assertEquals(
        'in1=foo&in2=bar&in2=baaz&in3=&pass=bar&textarea=foo%20bar%20baz&' +
            'select1=1&select2=a&select2=c&select3=&checkbox1=on&radio=X&radio2=Y',
        result);
  },

  testGetFormDataMap() {
    const el = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getFormDataMap(el);

    assertArrayEquals(['foo'], result.get('in1'));
    assertArrayEquals(['bar', 'baaz'], result.get('in2'));
    assertArrayEquals(['1'], result.get('select1'));
    assertArrayEquals(['a', 'c'], result.get('select2'));
    assertArrayEquals(['on'], result.get('checkbox1'));
    assertUndefined(result.get('select6'));
    assertUndefined(result.get('checkbox2'));
    assertArrayEquals(['X'], result.get('radio'));
    assertArrayEquals(['Y'], result.get('radio2'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHasFileInput() {
    let el = dom.getElement('testform1');
    assertFalse(forms.hasFileInput(el));
    el = dom.getElement('testform2');
    assertTrue(forms.hasFileInput(el));
  },

  testGetValueOnAtypicalValueElements() {
    let el = dom.getElement('testdiv1');
    let result = forms.getValue(el);
    assertNull(result);
    el = dom.getElement('testfieldset1');
    result = forms.getValue(el);
    assertNull(result);
    el = dom.getElement('testlegend1');
    result = forms.getValue(el);
    assertNull(result);
  },

  testHasValueInput() {
    const el = dom.getElement('in1');
    const result = forms.hasValue(el);
    assertTrue(result);
  },

  testGetValueByNameForNonExistentElement() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getValueByName(form, 'non_existent');
    assertNull(result);
  },

  testHasValueByNameInput() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'in1');
    assertTrue(result);
  },

  testHasValueInputEmpty() {
    const el = dom.getElement('in3');
    const result = forms.hasValue(el);
    assertFalse(result);
  },

  testHasValueByNameEmpty() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'in3');
    assertFalse(result);
  },

  testHasValueRadio() {
    const el = dom.getElement('radio1');
    const result = forms.hasValue(el);
    assertTrue(result);
  },

  testHasValueByNameRadio() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'radio');
    assertTrue(result);
  },

  testHasValueRadioNotChecked() {
    const el = dom.getElement('radio2');
    const result = forms.hasValue(el);
    assertFalse(result);
  },

  testHasValueByNameRadioNotChecked() {
    const form = dom.getElement('testform3');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'radio3');
    assertFalse(result);
  },

  testHasValueSelectSingle() {
    const el = dom.getElement('select1');
    const result = forms.hasValue(el);
    assertTrue(result);
  },

  testHasValueByNameSelectSingle() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'select1');
    assertTrue(result);
  },

  testHasValueSelectMultiple() {
    const el = dom.getElement('select2');
    const result = forms.hasValue(el);
    assertTrue(result);
  },

  testHasValueByNameSelectMultiple() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'select2');
    assertTrue(result);
  },

  testHasValueSelectNotSelected() {
    // select without value
    const el = dom.getElement('select3');
    const result = forms.hasValue(el);
    assertFalse(result);
  },

  testHasValueByNameSelectNotSelected() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'select3');
    assertFalse(result);
  },

  testHasValueSelectMultipleNotSelected() {
    const el = dom.getElement('select6');
    const result = forms.hasValue(el);
    assertFalse(result);
  },

  testHasValueByNameSelectMultipleNotSelected() {
    const form = dom.getElement('testform3');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.hasValueByName(form, 'select6');
    assertFalse(result);
  },

  // TODO(user): make this a meaningful selenium test
  testSetDisabledFalse() {},

  testSetDisabledTrue() {},

  // TODO(user): make this a meaningful selenium test
  testFocusAndSelect() {
    const el = dom.getElement('in1');
    forms.focusAndSelect(el);
  },

  testGetValueInput() {
    const el = dom.getElement('in1');
    const result = forms.getValue(el);
    assertEquals('foo', result);
  },

  testSetValueInput() {
    const el = dom.getElement('in3');
    forms.setValue(el, 'foo');
    assertEquals('foo', forms.getValue(el));

    forms.setValue(el, 3500);
    assertEquals('3500', forms.getValue(el));

    forms.setValue(el, 0);
    assertEquals('0', forms.getValue(el));

    forms.setValue(el, null);
    assertEquals('', forms.getValue(el));

    forms.setValue(el, undefined);
    assertEquals('', forms.getValue(el));

    forms.setValue(el, false);
    assertEquals('false', forms.getValue(el));

    forms.setValue(el, {});
    assertEquals({}.toString(), forms.getValue(el));

    forms.setValue(el, {
      toString: function() {
        return 'test';
      }
    });
    assertEquals('test', forms.getValue(el));

    // unset
    forms.setValue(el);
    assertEquals('', forms.getValue(el));
  },

  testGetValueMeter() {
    const el = dom.createDom('meter', {'min': 0, 'max': 3, 'value': 2.3});
    assertEquals(2.3, forms.getValue(el));
  },

  testSetValueMeter() {
    const el = dom.createDom('meter', {'min': 1, 'max': 5, 'value': 3});

    assertEquals(3, forms.getValue(el));

    forms.setValue(el, 2);
    assertEquals(2, forms.getValue(el));
  },

  testGetValuePassword() {
    const el = dom.getElement('pass');
    const result = forms.getValue(el);
    assertEquals('bar', result);
  },

  testGetValueByNamePassword() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getValueByName(form, 'pass');
    assertEquals('bar', result);
  },

  testGetValueTextarea() {
    const el = dom.getElement('textarea1');
    const result = forms.getValue(el);
    assertEquals('foo bar baz', result);
  },

  testGetValueByNameTextarea() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getValueByName(form, 'textarea1');
    assertEquals('foo bar baz', result);
  },

  testSetValueTextarea() {
    const el = dom.getElement('textarea2');
    forms.setValue(el, 'foo bar baz');
    const result = forms.getValue(el);
    assertEquals('foo bar baz', result);
  },

  testGetValueSelectSingle() {
    const el = dom.getElement('select1');
    const result = forms.getValue(el);
    assertEquals('1', result);
  },

  testGetValueByNameSelectSingle() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getValueByName(form, 'select1');
    assertEquals('1', result);
  },

  testSetValueSelectSingle() {
    const el = dom.getElement('select4');
    forms.setValue(el, '2');
    let result = forms.getValue(el);
    assertEquals('2', result);
    // unset
    forms.setValue(el);
    result = forms.getValue(el);
    assertNull(result);
  },

  testSetValueSelectSingleEmptyString() {
    const el = dom.getElement('select7');
    // unset
    forms.setValue(el);
    let result = forms.getValue(el);
    assertNull(result);
    forms.setValue(el, '');
    result = forms.getValue(el);
    assertEquals('', result);
  },

  testGetValueSelectMultiple() {
    const el = dom.getElement('select2');
    const result = forms.getValue(el);
    assertArrayEquals(['a', 'c'], result);
  },

  testGetValueSelectMultipleNotSelected() {
    const el = dom.getElement('select6');
    const result = forms.getValue(el);
    assertNull(result);
  },

  testGetValueByNameSelectMultiple() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = forms.getValueByName(form, 'select2');
    assertArrayEquals(['a', 'c'], result);
  },

  testSetValueSelectMultiple() {
    const el = dom.getElement('select5');
    forms.setValue(el, ['a', 'c']);
    let result = forms.getValue(el);
    assertArrayEquals(['a', 'c'], result);

    forms.setValue(el, 'a');
    result = forms.getValue(el);
    assertArrayEquals(['a'], result);

    // unset
    forms.setValue(el);
    result = forms.getValue(el);
    assertNull(result);
  },

  testGetValueCheckbox() {
    let el = dom.getElement('checkbox1');
    let result = forms.getValue(el);
    assertEquals('on', result);
    el = dom.getElement('checkbox2');
    result = forms.getValue(el);
    assertNull(result);
  },

  testGetValueByNameCheckbox() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    let result = forms.getValueByName(form, 'checkbox1');
    assertEquals('on', result);
    /** @suppress {checkTypes} suppression added to enable type checking */
    result = forms.getValueByName(form, 'checkbox2');
    assertNull(result);
  },

  testGetValueRadio() {
    let el = dom.getElement('radio1');
    let result = forms.getValue(el);
    assertEquals('X', result);
    el = dom.getElement('radio2');
    result = forms.getValue(el);
    assertNull(result);
  },

  testGetValueByNameRadio() {
    const form = dom.getElement('testform1');
    /** @suppress {checkTypes} suppression added to enable type checking */
    let result = forms.getValueByName(form, 'radio');
    assertEquals('X', result);

    /** @suppress {checkTypes} suppression added to enable type checking */
    result = forms.getValueByName(form, 'radio2');
    assertEquals('Y', result);
  },

  testGetValueButton() {
    const el = dom.getElement('button');
    const result = forms.getValue(el);
    assertEquals('button', result);
  },

  testGetValueSubmit() {
    const el = dom.getElement('submit');
    const result = forms.getValue(el);
    assertEquals('submitv', result);
  },

  testGetValueReset() {
    const el = dom.getElement('reset');
    const result = forms.getValue(el);
    assertEquals('reset', result);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testGetFormDataHelperAndNonInputElements() {
    const el = dom.getElement('testform4');
    forms.getFormDataHelper_(el, {}, goog.nullFunction);
  },
});
