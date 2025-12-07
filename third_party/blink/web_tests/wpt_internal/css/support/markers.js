function createRangeForTextOnly(element, start, end) {
  const textNode = element.firstChild;
  if (element.childNodes.length != 1 || textNode.nodeName != '#text') {
    throw new Error('element must contain a single #text node only');
  }
  const range = document.createRange();
  range.setStart(textNode, start);
  range.setEnd(textNode, end);
  return range;
}

function addMarker(element, start, end, type) {
  const range = createRangeForTextOnly(element, start, end);
  if (typeof internals !== 'undefined') {
    // To run this test from content_shell you can use
    // "--expose-internals-for-testing" command flag.
    internals.setMarker(document, range, type);
  }
}

function addSpellingMarker(element, start, end) {
  addMarker(element, start, end, 'spelling');
}

function addGrammarMarker(element, start, end) {
  addMarker(element, start, end, 'grammar');
}

function addSearchTextMarker(element, start, end, current = false) {
  const range = createRangeForTextOnly(element, start, end);
  if (typeof internals !== 'undefined') {
    // To run this test from content_shell you can use
    // "--expose-internals-for-testing" command flag.
    internals.addTextMatchMarker(range, current ? 'kActive' : 'kInactive');
  }
}

function setSelection(element, start, end) {
  const selection = getSelection();

  // Deselect any ranges that happen to be selected, to prevent the
  // Selection#addRange call from ignoring our new range (see
  // <https://www.chromestatus.com/feature/6680566019653632> for
  // more details).
  selection.removeAllRanges();

  selection.addRange(createRangeForTextOnly(element, start, end));
}
