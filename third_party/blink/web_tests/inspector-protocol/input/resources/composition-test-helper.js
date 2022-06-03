(class CompositionTestHelper {
  static async init(session, inputValue) {
    await session.evaluate(`
    window.input = document.createElement('input');
    document.body.appendChild(window.input);
    window.input.focus();
    window.input.value = '` + inputValue + `';

    window.logs = [];

    logs.push('Initial value:' + JSON.stringify(window.input.value));

    window.input.addEventListener('compositionupdate', logEvent);
    window.input.addEventListener('input', logEvent);
    window.input.addEventListener('compositionstart', logEvent);
    window.input.addEventListener('compositionend', logEvent);
    window.input.addEventListener('beforeinput', logEvent);
    window.addEventListener('keydown', logEvent);
    window.addEventListener('keypress', logEvent);
    window.addEventListener('keyup', logEvent);

    function logEvent(event) {
      logs.push('');
      logs.push('event: ' + event.type);
      logs.push('value: ' + input.value);
      logs.push('selectionStart: ' + input.selectionStart);
      logs.push('selectionEnd: ' + input.selectionEnd);
      if (event.keyCode)
        logs.push('keyCode: ' + event.keyCode);
      if (event.key)
        logs.push('key: ' + event.key);
      if (event.charCode)
        logs.push('charCode: ' + event.charCode);
      if (event.text)
        logs.push('text: ' + event.text);
      if (event.location)
        logs.push('location: ' + event.location);
      if (event.code)
        logs.push('code: ' + event.code);
    }
  `);
  }
})
