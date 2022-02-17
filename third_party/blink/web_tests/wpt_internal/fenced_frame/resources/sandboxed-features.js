async function test_prompt() {
  assert_equals(
    window.prompt('Test prompt'),
    null,
    'window.prompt() must synchronously return null in a fenced frame without' +
    ' blocking on user input.');
}

async function test_alert() {
  assert_equals(
    window.alert('Test alert'),
    undefined,
    'window.alert() must synchronously return undefined in a fenced frame' +
    '  without blocking on user input.');
}

async function test_confirm() {
  assert_equals(
    window.confirm('Test confirm'),
    false,
    'window.confirm() must synchronously return false in a fenced frame' +
    ' without blocking on user input.');
}

async function test_print() {
  assert_equals(
    window.print(),
    undefined,
    'window.print() must synchronously return undefined in a fenced frame' +
    ' without blocking on user input.');

  assert_equals(
    document.execCommand('print', false, null),
    false,
    'execCommand(\'print\') must synchronously return false in a fenced frame' +
    ' without blocking on user input.');
}

async function test_document_domain() {
  assert_throws_dom('SecurityError', () => {
    document.domain = 'example.test';
  });
  assert_throws_dom('SecurityError', () => {
    document.domain = document.domain;
  });
  assert_throws_dom('SecurityError', () => {
    (new Document).domain = document.domain;
  });
  assert_throws_dom('SecurityError', () => {
    document.implementation.createHTMLDocument().domain = document.domain;
  });
  assert_throws_dom('SecurityError', () => {
    document.implementation.createDocument(null, '').domain = document.domain;
  });
  assert_throws_dom('SecurityError', () => {
    document.createElement('template').content.ownerDocument.domain =
        document.domain;
  });
}

async function test_presentation_request() {
  assert_throws_dom('SecurityError', () => {
    new PresentationRequest([location.href]);
  });
}

async function test_screen_orientation_lock() {
  try {
    await screen.orientation.lock('portrait');
  } catch (e) {
    assert_equals(
      e.name,
      'SecurityError',
      'orientation.lock() must throw a SecurityError in a fenced frame.');
    return;
  }
  assert_unreached('orientation.lock() must throw an error');
}

async function test_pointer_lock() {
  await new Promise(resolve => requestAnimationFrame(resolve));
  await new Promise(resolve => simulateGesture(resolve));

  const canvas = document.createElement('canvas');
  document.body.appendChild(canvas);
  const pointerlockerror_promise = new Promise(resolve => {
    document.addEventListener('pointerlockerror', resolve);
  });
  try {
    await canvas.requestPointerLock();
  } catch (e) {
    assert_equals(
      e.name,
      'SecurityError',
      'orientation.lock() must throws a SecurityError in a fenced frame.');
    await pointerlockerror_promise;
    return;
  }
  assert_unreached('requestPointerLock() must fail in a fenced frame');
}