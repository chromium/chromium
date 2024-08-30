'use strict';

// This set of utils also requires the inclusion of
// third_party/blink/web_tests/shadow-dom/resources/shadow-dom.js.

function innermostActiveElement(element) {
  element = element || document.activeElement;
  if (isIFrameElement(element)) {
    if (element.contentDocument.activeElement)
      return innermostActiveElement(element.contentDocument.activeElement);
    return element;
  }
  if (isShadowHost(element)) {
    let shadowRoot = internals.shadowRoot(element);
    if (shadowRoot) {
      if (shadowRoot.activeElement)
        return innermostActiveElement(shadowRoot.activeElement);
    }
  }
  return element;
}

function isInnermostActiveElement(path) {
  const element = getNodeInComposedTree(path);
  if (!element)
    return false;
  return element === innermostActiveElement();
}

function shouldNavigateFocus(from, direction) {
  const fromElement = getNodeInComposedTree(from);
  if (!fromElement)
    return false;

  fromElement.focus();
  if (!isInnermostActiveElement(from))
    return false;

  if (direction == 'forward')
    navigateFocusForward();
  else
    navigateFocusBackward();

  return true;
}

function navigateFocusForward() {
  if (window.eventSender)
    eventSender.keyDown('\t');
}

function navigateFocusBackward() {
  if (window.eventSender)
    eventSender.keyDown('\t', ['shiftKey']);
}

function assert_focus_navigation(from, to, direction) {
  const result = shouldNavigateFocus(from, direction);
  assert_true(result, 'Failed to focus ' + from);
  const message =
    'Focus should move ' + direction + ' from ' + from + ' to ' + to;
  var toElement = getNodeInComposedTree(to);
  assert_equals(innermostActiveElement(), toElement, message);
}

function assert_focus_navigation_forward(elements) {
  assert_true(
    elements.length >= 2,
    'length of elements should be greater than or equal to 2.');
  for (var i = 0; i + 1 < elements.length; ++i)
    assert_focus_navigation(elements[i], elements[i + 1], 'forward');
}

function assert_focus_navigation_backward(elements) {
  assert_true(
    elements.length >= 2,
    'length of elements should be greater than or equal to 2.');
  for (var i = 0; i + 1 < elements.length; ++i)
    assert_focus_navigation(elements[i], elements[i + 1], 'backward');
}

function assert_focus_navigation_bidirectional(elements) {
  assert_focus_navigation_forward(elements);
  elements.reverse();
  assert_focus_navigation_backward(elements);
}
