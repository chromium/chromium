class EmptyOperation {
  async run(data) {}
}

/* dummy text */console.log(`loaded module, test token: ${typeof(testToken) === 'undefined' ? '<undefined>' : testToken}`);
var globalVar = 0;

class SetGlobalVarAndPauseOnDebuggerOperation {
  async run(data) {
    if (data && data.hasOwnProperty('setGlobalVarTo')) {
      globalVar = data['setGlobalVarTo'];
    }

    debugger;
    globalVar = 100;
  }
}

register("empty-operation", EmptyOperation);
register("set-global-var-and-pause-on-debugger-operation",
         SetGlobalVarAndPauseOnDebuggerOperation);