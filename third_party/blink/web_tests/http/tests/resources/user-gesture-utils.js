/**
 * Simulates a user click on coordinates [x], [y].
 * For example, for vibrate to be allowed:
 * https://www.chromestatus.com/feature/5644273861001216.
 */
function simulateUserClick(x, y) {
  if (window.eventSender) {
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
  }
}

/**
 * Calls `focus()` on |w|, but via a user gesture.
 * This is needed to move system focus to another window, as we drop
 * the request in the renderer without a user gesture.
 */
function focusWithUserGesture(w) {
  var focus_w = document.createElement('a');
  focus_w.onclick = () => { w.focus(); };
  focus_w.innerText = 'focus_w';
  document.body.appendChild(focus_w);
  simulateUserClick(focus_w.getBoundingClientRect().x,
                    focus_w.getBoundingClientRect().y);
  document.body.removeChild(focus_w);
}

/**
 * Ensures any events that involve the browser have a chance to be processed
 * and replied to, then calls the given function.
 * When testing focus, this is useful to avoid long timeouts waiting to see
 * if a focus event will be dispatched or not.
 */
function roundTripToBrowser() {
  return new Promise((resolve, reject) => {
    var frame = document.createElement('iframe');
    // An OOPIF navigation requires the browser to participate.
    frame.src = "http://localhost:8080/resources/blank.html";
    frame.addEventListener('load', () => {
      document.body.removeChild(frame);
      requestAnimationFrame(() => resolve());
    });
    document.body.appendChild(frame);
  });
}
