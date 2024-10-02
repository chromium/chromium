const testSelectOptionText = `Long options, wider than select`;
const frameWidth = 500;
const frameHeight = 300;

async function createFrameWithContent(content) {
  const frame = document.createElement('iframe');
  frame.width = frameWidth;
  frame.height = frameHeight;
  frame.srcdoc = content;
  const loaded = new Promise(resolve => frame.addEventListener('load',resolve));
  document.body.appendChild(frame);
  await loaded;
  return frame.contentDocument;
}

async function wait2Frames(doc) {
  await new Promise(resolve => doc.defaultView.requestAnimationFrame(resolve));
  await new Promise(resolve => doc.defaultView.requestAnimationFrame(resolve));
}

async function scroll(doc,x,y) {
  doc.defaultView.scrollTo({left: x, top: y, behavior: "instant"});
  await wait2Frames(doc);
}

async function capture(doc) {
  await wait2Frames(doc);
  document.documentElement.classList.remove('reftest-wait');
}

const commonStyleBlock = `
  html {
    scrollbar-width: none;
  }
  body {
    width: 2000px;
    height: 2000px;
  }
  .select {
    position: relative;
    top: 1000px;
    left: 600px;
  }
  .select,::picker(select) {
    appearance:base-select;
  }
`;

async function generateTestFrame(numOptions,initialx,initialy) {
  const singleOption = `<option>${testSelectOptionText}</option>`
  const options = Array(numOptions).fill(singleOption).join('\n');
  const content = `
    <select class="select">
      <option value="" selected>Select</option>
      ${options}
    </select>
    <style>
    ${commonStyleBlock}
    </style>
  `;
  const doc = await createFrameWithContent(content);

  await scroll(doc,initialx,initialy);
  await wait2Frames(doc);
  await test_driver.bless();
  doc.querySelector('select').showPicker();
  await capture(doc);
}

async function generateReferenceFrame(numOptions,initialx,initialy,extraStyleRules) {
  const singleOption = `<div tabindex=0 class="customizable-select-option">${testSelectOptionText}</div>`
  const options = Array(numOptions).fill(singleOption).join('\n');
  const content = `
    <link rel=stylesheet href="resources/customizable-select-styles.css">
    <div class="select customizable-select-button" popovertarget=popover id=button style="anchor-name:--button">
      <span class=customizable-select-selectedoption>Select</span>
    </div>
    <div id=popover popover=auto anchor=button class=customizable-select-popover style="position-anchor:--button">
      <div tabindex=0 autofocus class="customizable-select-option selected">Select</div>
      ${options}
    </div>
    <style>
    ${commonStyleBlock}
    ${extraStyleRules}
    </style>
    <script>document.querySelector("[popover]").showPopover()</script>
  `;
  const doc = await createFrameWithContent(content);
  await scroll(doc,initialx,initialy);
  await capture(doc);
}
