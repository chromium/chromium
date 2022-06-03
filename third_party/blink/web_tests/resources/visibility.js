'use strict';

function setMainWindowHidden(hidden) {
  return new Promise((resolve, reject) => {
    if (!window.testRunner) {
      reject("no window.testRunner present");
      return;
    }
    if (document.visibilityState == (hidden ? "hidden" : "visible")) {
      reject("setMainWindowHidden(" + hidden + ") called but already " + hidden);
      return;
    }
    document.addEventListener("visibilitychange", resolve, {once:true});
    testRunner.setMainWindowHidden(hidden);
  });
}
