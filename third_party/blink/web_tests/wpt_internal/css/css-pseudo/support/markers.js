function addMarker(element, start, end, type) {
  const range = document.createRange();
  const textNode = element.firstChild;
  range.setStart(textNode, start);
  range.setEnd(textNode, end);
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
