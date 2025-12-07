(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test reported issues for customizable <select> accessibility');

  await dp.Audits.enable();

  const audits = [];
  dp.Audits.onIssueAdded(issue => {
    if (issue.params.issue.code === 'ElementAccessibilityIssue') {
      audits.push(issue)
    }
  });

  await page.loadHTML(`
    <!DOCTYPE html>
    <link rel=author href="mailto:ansollan@microsoft.com">
    <link rel=help href="https://issues.chromium.org/issues/347890366">

    <style>
      select, ::picker(select) {
        appearance: base-select;
      }
    </style>

    <!-- Should print two console messages. -->
    <select>
      <label>label</label>
    </select>

    <!-- Should print two console messages. -->
    <select>
      <a href="https://www.example.com"></a>
      <a href="https://www.example.com"></a>
    </select>

    <!-- Should print one console message. -->
    <select>
      <option>
        <button></button>
      </option>
    </select>

    <!-- Should print one console message. -->
    <select>
      <option>
        <datalist>
        </datalist>
      </option>
    </select>

    <!-- Should print one console message. -->
    <select>
      <option>
        <object>
        </object>
      </option>
    </select>

    <!-- Phrasing content but not Interactive content allowed. -->
     <!-- Should print one console message. -->
    <select>
      <option>
        <textarea></textarea>
      </option>
    </select>

    <!-- <button> (if present) should be the first child of <select>. -->
    <!-- Should print one console message. -->
    <select>
      <option></option>
      <option></option>
      <button>button</button>
    </select>

    <!-- Should print two console messages. -->
    <select>
      <option></option>
      <div>
        <button>button</button>
      </div>
      <span>
        <button>button</button>
      </span>
    </select>

    <!-- <legend> (if present) should be the first child of <optgroup>. -->
    <!-- Should print one console message. -->
    <select>
      <optgroup>
        <option></option>
        <legend>legend</legend>
      </optgroup>
    </select>

    <!-- Should print two console messages. -->
    <select>
      <optgroup>
        <div>
          <legend>legend</legend>
        </div>
      </optgroup>
      <optgroup>
        <span>
          <legend>legend</legend>
        </span>
      </optgroup>
    </select>

    <!-- <legend> elements should not have interactive elements inside -->
    <!-- Should print one console message. -->
    <select>
      <optgroup>
          <legend>
            <button></button>
          </legend>
      </optgroup>
    </select>

    <!-- <optgroup> should not have interactive elements inside. -->
    <!-- Should print one console message. -->
    <select>
      <optgroup>
        <option>..</option>
        <a href="https://www.example.com"></a>
      </optgroup>
    </select>

    <!-- Should print two console messages. -->
    <select>
      <div>
        <option>
          <span tabindex="1"></span>
          <span tabindex="2"></span>
        </option>
      </div>
    </select>

    <!-- Should print two console messages. -->
    <select>
      <div>
        <option>
          <span contenteditable="true">..</span>
          <span contenteditable="plaintext-only">..</span>
        </option>
      </div>
    </select>

    <!-- Should print one console message. -->
    <select>
      <label is="custom-label"></label>
    </select>

    <script>
      class CustomLabel extends HTMLLabelElement {
        constructor() {
          super();
          this.style.color = 'blue';
          this.style.fontWeight = 'bold';
        }
      }
      customElements.define('custom-label', CustomLabel, { extends: 'label' });
    </script>
  `);

  testRunner.log(audits);
  testRunner.completeTest();
});
