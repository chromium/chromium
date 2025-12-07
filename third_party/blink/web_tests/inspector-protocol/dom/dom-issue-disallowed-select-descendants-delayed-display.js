(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test reported issues for customizable <select> accessibility');

  await dp.Audits.enable();

  const audits = [];
  let count = 0;
  let expected_count = 5;
  dp.Audits.onIssueAdded(issue => {
    if (issue.params.issue.code === 'ElementAccessibilityIssue') {
      audits.push(issue)
      if (++count == expected_count) {
        requestAnimationFrame(() => {
          testRunner.log(audits);
          testRunner.completeTest();
        });
      }
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

    <!-- Should print one console message. -->
    <select class="delayed" style="display:none">
      <textarea></textarea>
      <option></option>
    </select>

    <!-- Should print two console messages. -->
    <select class="delayed" style="display:none">
      <div>
        <option>
          <span tabindex="1">..</span>
          <span tabindex="2">..</span>
        </option>
      </div>
    </select>

    <!-- Should print two console messages. -->
    <select class="delayed" style="display:none">
      <div>
        <option>
          <span contenteditable="true">..</span>
          <span contenteditable="plaintext-only">..</span>
        </option>
      </div>
    </select>

    <script>
      requestAnimationFrame(() => {
        document.querySelectorAll('.delayed').forEach((select) => {
          select.style.display = '';
        });
      });
    </script>
  `);
});
