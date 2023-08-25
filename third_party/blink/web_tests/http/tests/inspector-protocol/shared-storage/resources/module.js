var globalVar = 0;

class EmptyOperation {
  async run(data) {}
}

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