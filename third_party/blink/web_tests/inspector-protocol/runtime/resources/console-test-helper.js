(class ConsoleTestHelper{
  constructor(testRunner, dp, evaluate) {
    this._testRunner = testRunner;
    this._dp = dp;
    this._evaluate = evaluate;
  }

  async testConsoleAssert(contextId) {
    await this._evaluate(`console.assert(false, 10, Infinity, {a:3, b:'hello'})`, contextId);
    await this._evaluate(`console.assert(true, 10, Infinity, {a:3, b:'hello'})`, contextId); // should not fire ConsoleAPICalled event
    await this._evaluate(`console.assert(10, Infinity, {a:3, b:'hello'})`, contextId); // should not fire ConsoleAPICalled event
    await this._evaluate('console.assert()', contextId);
  }
  async testConsoleTime(contextId) {
    await this._evaluate('console.time()', contextId);
    await this._evaluate('console.time(10)', contextId);
    await this._evaluate('console.time(NaN)', contextId);
    await this._evaluate(`console.time({a:3, b:'hello'})`, contextId);
    this._testRunner.log('console.time with duplicate label');
    await this._evaluate('console.time(10)', contextId);
    await this._evaluate('console.timeEnd()', contextId);
    await this._evaluate('console.timeEnd(10)', contextId);
    await this._evaluate('console.timeEnd(NaN)', contextId);
    this._testRunner.log('console.timeEnd object label');
    await this._evaluate(`console.timeEnd({a:3, b:'hello'})`, contextId);
    this._testRunner.log('console.timeEnd unused label');
    await this._evaluate('console.timeEnd(9)', contextId);
    this._testRunner.log('console.time/timeEnd multiple args');
    await this._evaluate(`console.time(5, {a:2, b:'hi'}, Infinity)`, contextId); //Should ignore all params except the first
    await this._evaluate(`console.timeEnd({a:3, b:'hello'}, 5)`, contextId); // Should produce warning
    await this._evaluate(`console.timeEnd(5, Infinity, {a:3, b:'hello'})`, contextId);
  }

  async testConsoleCount(contextId) {
    //Clearing counts
    await this._evaluate('console.countReset()', contextId);
    await this._evaluate('console.countReset(2)', contextId);
    await this._evaluate('console.countReset(3)', contextId);
    await this._evaluate('console.countReset(4)', contextId);
    await this._evaluate('console.countReset(10)', contextId);
    await this._evaluate('console.countReset(NaN)', contextId);
    await this._evaluate(`console.countReset({a:3, b:'hello'})`, contextId);
    this._testRunner.log('blank label');
    await this._evaluate('console.count()', contextId);
    this._testRunner.log('primitive label');
    await this._evaluate('console.count(10)', contextId);
    this._testRunner.log('unserializable label');
    await this._evaluate('console.count(NaN)', contextId);
    this._testRunner.log('object label');
    await this._evaluate(`console.count({a:3, b:'hello'})`, contextId);
    this._testRunner.log('incrementing on NaN');
    await this._evaluate('console.count(NaN)', contextId);
    await this._evaluate('console.count(NaN)', contextId);
    await this._evaluate('console.count(NaN)', contextId);
    this._testRunner.log('incrementing on "default" label');
    await this._evaluate('console.count()', contextId);
    await this._evaluate('console.count(undefined)', contextId);
    await this._evaluate('console.count("default")', contextId);
    this._testRunner.log('console.countReset for default label');
    await this._evaluate('console.countReset()', contextId);
    await this._evaluate('console.count()', contextId);
    this._testRunner.log('console.countReset for primitive label');
    await this._evaluate('console.countReset(10)', contextId);
    await this._evaluate('console.count(10)', contextId);
    this._testRunner.log('console.countReset for unserializable label');
    await this._evaluate('console.countReset(NaN)', contextId);
    await this._evaluate('console.count(NaN)', contextId);
    this._testRunner.log('console.countReset for object label');
    await this._evaluate(`console.countReset({a:3, b:'hello'})`, contextId);
    await this._evaluate(`console.count({a:3, b:'hello'})`, contextId);
    this._testRunner.log('console.count incrementing post reset');
    await this._evaluate('console.count(NaN)', contextId);
    this._testRunner.log('console.count/countReset for multiple labels');
    await this._evaluate('console.count(2, 3, 4)', contextId);
    await this._evaluate('console.count(3, 2, 4)', contextId);
    await this._evaluate('console.count(2)', contextId);
    await this._evaluate('console.countReset(2, 3, 4)', contextId);
    await this._evaluate('console.count(2, 3, 4)', contextId);
    await this._evaluate('console.count(2)', contextId);
    await this._evaluate('console.count(3)', contextId);
  }

  validateApiNotFound(evalResult) {
    const className = evalResult.result.result.className;
    if (className !== 'ReferenceError') {
      this._testRunner.log(`Unexpected non-error: ${evalResult}`);
    } else {
      this._testRunner.log(`Expected ReferenceError: ${className}`);
    }
  }

  async testDir() {
    this._testRunner.log('dir(document.body), includeCommandLineAPI = true.');
    await this._evaluate('dir(document.body)', { includeCommandLineAPI: true });


    this._testRunner.log('dir(document.body), includeCommandLineAPI = false.');
    let result = await this._evaluate('dir(document.body)', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);
  }
  async test$() {
    this._testRunner.log('$("body"), includeCommandLineAPI = true.');
    let result = await this._evaluate('$("body")', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('$("body"), includeCommandLineAPI = false.');
    result = await this._evaluate('$("body")', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);
  }
  async test$$() {
    this._testRunner.log('$$("head"), includeCommandLineAPI = true.');
    let result = await this._evaluate('$$("head")', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('$$("head"), includeCommandLineAPI = false.');
    result = await this._evaluate('$$("head")', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

  }
  async testKeys() {
    this._testRunner.log('JSON.stringify(keys({"1": 123, "2":567})), includeCommandLineAPI = true.');
    let result = await this._evaluate(' JSON.stringify(keys({"1": 123, "2":567}))', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('keys({"1": 123, "2":567}), includeCommandLineAPI = false.');
    result = await this._evaluate('keys({"1": 123, "2":567})', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

  }
  async testValues() {
    this._testRunner.log('JSON.stringify(values({"1": 123, "2":567})), includeCommandLineAPI = true.');
    let result = await this._evaluate(' JSON.stringify(values({"1": 123, "2":567}))', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('values({"1": 123, "2":567}) includeCommandLineAPI = false.');
    result = await this._evaluate('values({"1": 123, "2":567})', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

  }
  async test$x() {
    this._testRunner.log('$x("//body"), includeCommandLineAPI = true.');
    let result = await this._evaluate('$x("//body")', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('$x("//body") includeCommandLineAPI = false.');
    result = await this._evaluate('$x("//body")', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

  }
  async test$_() {
    this._testRunner.log('set $_ to 405, includeCommandLineAPI = true.');
    await this._evaluate('400+5', { includeCommandLineAPI: true, 'objectGroup': 'console' });
    let result = await this._evaluate('$_', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('evaluate $_, includeCommandLineAPI = false.');
    result = await this._evaluate('$_', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

  }
  async test$0to$4() {
    const docResultNode = await this._dp.DOM.getDocument({depth: 3});
    const rootNode = docResultNode.result.root;
    let nodeIds = [];
    let result;
    nodeIds.push(rootNode.nodeId);
    this._testRunner.log('initial $0-$4 are undefined, includeCommandLineAPI = true.');
    for (let i = 0; i < 5; i++) {
      result = await this._evaluate('$0', { includeCommandLineAPI: true });
      this._testRunner.log(`$${i} : ${result.result.result.type}`);
    }

    await this._dp.DOM.setInspectedNode({ 'nodeId': nodeIds[0] });
    let n;
    let i = 0;
    for (n of rootNode.children) {
      if (i >= 4) {
        break;
      }
      nodeIds.push(n.nodeId);
      i += 1;
    }
    this._testRunner.log('evaluate $0, includeCommandLineAPI = true.');
    result = await this._evaluate('tempTestVar = $0', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('evaluate $0, includeCommandLineAPI = false.');
    result = await this._evaluate('$0', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

    await this._dp.DOM.setInspectedNode({ 'nodeId': nodeIds[i] });
    this._testRunner.log('assign $1, includeCommandLineAPI = true.');
    result = await this._evaluate('tempTestVar === $1', { includeCommandLineAPI: true });
    this._testRunner.log(`previous $0 should equal $1: ${result.result.result.value}`);
  }
  async testGetEventListeners() {
    this._testRunner.log('set & get eventListener for document.onclick');
    await this._evaluate('handler = function() { return "clicked!" }; document.onclick = handler;', { includeCommandLineAPI: false });
    let result = await this._evaluate('JSON.stringify(getEventListeners(document))', { includeCommandLineAPI: true });
    const includesClick = (result.result.result.value).includes(`"click":`);
    this._testRunner.log(`Includes click handler: ${includesClick}`);

    this._testRunner.log('evaluate eventListener, includeCommandLineAPI = false.');
    result = await this._evaluate('getEventListeners(document)', { includeCommandLineAPI: false });
    this.validateApiNotFound(result);

    this._testRunner.log('eventListener with empty args, includeCommandLineAPI = true.');
    result = await this._evaluate('getEventListeners()', { includeCommandLineAPI: true });
    this._testRunner.log(result);

    this._testRunner.log('eventListener with invalid args, includeCommandLineAPI = false.');
    result = await this._evaluate('getEventListeners({})', { includeCommandLineAPI: true });
    this._testRunner.log(result);

  }

});
