// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * To have the IDE support for types when writing inspector-protocol tests:
 *
 * - `npm i devtools-protocol -g`
 * - `cd $HOME && npm link devtools-protocol`
 *
 * Note that `devtools-protocol` package won't include your local changes
 * to the protocol and might be slightly out-of-date. Update it from time to time.
 */
var TestRunner = class {
  constructor(testBaseURL, targetBaseURL, log, completeTest, fetch, params) {
    this._dumpInspectorProtocolMessages = false;
    this._protocolTimeout = 0;
    this._testBaseURL = testBaseURL;
    this._targetBaseURL = targetBaseURL;
    this._log = log;
    this._completeTest = completeTest;
    this._fetch = fetch;
    this._params = params;
    this._browserSession = new TestRunner.Session(this, '');
    this._stableValues = new Map();
  }

  static get stabilizeNames() {
    return [
      'id',
      'nodeId',
      'objectId',
      'scriptId',
      'timestamp',
      'backendNodeId',
      'parentId',
      'frameId',
      'loaderId',
      'baseURL',
      'documentURL',
      'styleSheetId',
      'executionContextId',
      'executionContextUniqueId',
      'openerId',
      'targetId',
      'browserContextId',
      'sessionId',
      'receivedBytes',
      'ownerNode',
      'guid',
      'requestId',
      'openerFrameId',
      'issueId',
      'initiatingFrameId'
    ];
  }

  static extendStabilizeNames(extended) {
    return [
      ...TestRunner.stabilizeNames,
      ...extended
    ]
  };

  startDumpingProtocolMessages() {
    this._dumpInspectorProtocolMessages = true;
  };

  completeTest() {
    this._completeTest.call(null);
  }

  log(item, title, stabilizeNames, stabilizeValues) {
    if (typeof item === 'object')
      return this._logObject(item, title, stabilizeNames, stabilizeValues);
    this._log.call(null, item);
  }

  params(name) {
    if (name) {
      return this._params instanceof URLSearchParams
          ? this._params.get(name) : this._params[name];
    }

    return this._params;
  }

  _logObject(object, title, stabilizeNames = TestRunner.stabilizeNames, stabilizeValues = []) {
    var lines = [];
    const stableValues = this._stableValues;

    function dumpValue(value, prefix, prefixWithName) {
      if (typeof value === 'object' && value !== null) {
        if (value instanceof Array)
          dumpItems(value, prefix, prefixWithName);
        else
          dumpProperties(value, prefix, prefixWithName);
      } else {
        lines.push(prefixWithName + String(value).replace(/\n/g, ' '));
      }
    }

    function dumpProperties(object, prefix, firstLinePrefix) {
      prefix = prefix || '';
      firstLinePrefix = firstLinePrefix || prefix;
      lines.push(firstLinePrefix + '{');

      var propertyNames = Object.keys(object);
      propertyNames.sort();
      for (var i = 0; i < propertyNames.length; ++i) {
        var name = propertyNames[i];
        if (!object.hasOwnProperty(name))
          continue;
        var prefixWithName = '    ' + prefix + name + ' : ';
        var value = object[name];
        if (stabilizeValues && stabilizeValues.includes(name)) {
          if (!stableValues.has(value)) {
            stableValues.set(value, `<${typeof value} ${stableValues.size}>`);
          }
          value = stableValues.get(value);
        } else if (stabilizeNames && stabilizeNames.includes(name)) {
          value = `<${typeof value}>`;
        }
        dumpValue(value, '    ' + prefix, prefixWithName);
      }
      lines.push(prefix + '}');
    }

    function dumpItems(object, prefix, firstLinePrefix) {
      prefix = prefix || '';
      firstLinePrefix = firstLinePrefix || prefix;
      lines.push(firstLinePrefix + '[');
      for (var i = 0; i < object.length; ++i)
        dumpValue(object[i], '    ' + prefix, '    ' + prefix + '[' + i + '] : ');
      lines.push(prefix + ']');
    }

    dumpValue(object, '', title || '');
    this._log.call(null, lines.join('\n'));
  }

  trimURL(url) {
    return url.replace(/^.*(([^/]*[/]){3}[^/]*)$/, '...$1');
  }

  url(relative) {
    if (
      relative.startsWith('http://') ||
      relative.startsWith('https://') ||
      relative.startsWith('file://') ||
      relative.startsWith('chrome://') ||
      relative === 'about:blank'
    )
      return relative;
    return this._targetBaseURL + relative;
  }

  async runTestSuite(testSuite) {
    for (var test of testSuite) {
      this.log('\nRunning test: ' + test.name);
      try {
        await test();
      } catch (e) {
        this.log(`Error during test: ${e}\n${e.stack}`);
      }
    }
    this.completeTest();
  }

  _checkExpectation(fail, name, messageObject) {
    if (fail === !!messageObject.error) {
      this.log('PASS: ' + name);
      return true;
    }

    this.log('FAIL: ' + name + ': ' + JSON.stringify(messageObject));
    this.completeTest();
    return false;
  }

  expectedSuccess(name, messageObject) {
    return this._checkExpectation(false, name, messageObject);
  }

  expectedError(name, messageObject) {
    return this._checkExpectation(true, name, messageObject);
  }

  die(message, error) {
    this.log(`${message}: ${error}\n${error.stack}`);
    this.completeTest();
    throw new Error(message);
  }

  fail(message) {
    this.log('FAIL: ' + message);
    this.completeTest();
  }

  async loadScript(url) {
    var source = await this._fetch(this._testBaseURL + url);
    return eval(`${source}\n//# sourceURL=${url}`);
  };

  async loadScriptAbsolute(url) {
    var source = await this._fetch(url);
    return eval(`${source}\n//# sourceURL=${url}`);
  };

  async loadScriptModule(path) {
    const source = await this._fetch(this._testBaseURL + path);

    return new Promise((resolve, reject) => {
      const src = URL.createObjectURL(new Blob([source], { type: 'application/javascript' }));
      const script = Object.assign(document.createElement('script'), {
        src,
        type: 'module',
        onerror: reject,
        onload: resolve
      });

      document.head.appendChild(script);
    })
  };

  browserSession() {
    return this._browserSession;
  }

  browserP() {
    return this._browserSession.protocol;
  }

  async attachFullBrowserSession() {
    const bp = this._browserSession.protocol;
    const browserSessionId = (await bp.Target.attachToBrowserTarget()).result.sessionId;
    return new TestRunner.Session(this, browserSessionId);
  }

  async createPage(options) {
    options = options || {};
    const browserProtocol = this._browserSession.protocol;
    const params = {url: 'about:blank'};
    if (options.width)
      params.width = options.width;
    if (options.height)
      params.height = options.height;
    if (options.enableBeginFrameControl)
      params.enableBeginFrameControl = true;
    if (options.createContextOptions) {
      const browserContextId = (await browserProtocol.Target.createBrowserContext(options.createContextOptions)).result.browserContextId;
      params.browserContextId = browserContextId;
    }
    const targetId = (await browserProtocol.Target.createTarget(params)).result.targetId;
    const page = new TestRunner.Page(this, targetId);
    let url = options.url || DevToolsHost.dummyPageURL;
    if (!url) {
      url = window.location.href;
      url = url.substring(0, url.indexOf('inspector-protocol-test.html')) + 'inspector-protocol-page.html';
    }
    await page.navigate(url);
    return page;
  }

  async _start(description, options) {
    try {
      if (!description)
        throw new Error('Please provide a description for the test!');
      this.log(description);
      var page = await this.createPage(options);
      if (options.html)
        await page.loadHTML(options.html);
      var session = await page.createSession();
      return { page: page, session: session, dp: session.protocol };
    } catch (e) {
      this.die('Error starting the test', e);
    }
  };

  startBlank(description, options) {
    return this._start(description, options || {});
  }

  startHTML(html, description, options) {
    options = options || {};
    options.html = html;
    return this._start(description, options);
  }

  startURL(url, description, options) {
    options = options || {};
    options.url = url;
    return this._start(description, options);
  }

  startWithFrameControl(description, options) {
    options = options || {};
    options.width = options.width || 800;
    options.height = options.height || 600;
    options.createContextOptions = {};
    options.enableBeginFrameControl = true;
    return this._start(description, options);
  }

  async startBlankWithTabTarget(description) {
    try {
      if (!description)
        throw new Error('Please provide a description for the test!');
      this.log(description);

      const bp = this.browserP();
      const params = {url: 'about:blank', forTab: true};
      const tabTargetId =
          (await bp.Target.createTarget(params)).result.targetId;
      const tabTargetSessionId = (await bp.Target.attachToTarget({
          targetId: tabTargetId,
                                   flatten: true
                                 })).result.sessionId;
      const tabTargetSession = new TestRunner.Session(this, tabTargetSessionId);

      return {tabTargetSession};
    } catch (e) {
      this.die('Error starting the test', e);
    }
  }

  async logStackTrace(debuggers, stackTrace, debuggerId) {
    while (stackTrace) {
      const {description, callFrames, parent, parentId} = stackTrace;
      if (description)
        this.log(`--${description}--`);
      this.logCallFrames(callFrames);
      if (parentId) {
        if (parentId.debuggerId)
          debuggerId = parentId.debuggerId;
        let result = await debuggers.get(debuggerId).getStackTrace({
          stackTraceId: parentId
        });
        stackTrace = result.stackTrace || result.result.stackTrace;
      } else {
        stackTrace = parent;
      }
    }
  }

  _replaceUUID(url) {
    const uuidRegex = new RegExp('[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}');
    return url.replace(uuidRegex, 'UUID');
  }

  logCallFrames(callFrames) {
    for (let frame of callFrames) {
      let functionName = frame.functionName || '(anonymous)';
      let url = this._replaceUUID(frame.url);
      let location = frame.location || frame;
      this.log(`${functionName} at ${url}:${
                                            location.lineNumber
                                          }:${location.columnNumber}`);
    }
  }
};

TestRunner.Page = class {
  constructor(testRunner, targetId) {
    this._testRunner = testRunner;
    this._targetId = targetId;
  }

  targetId() {
    return this._targetId;
  }

  async createSession() {
    let dp = this._testRunner._browserSession.protocol;
    const sessionId = (await dp.Target.attachToTarget({targetId: this._targetId, flatten: true})).result.sessionId;
    return new TestRunner.Session(this._testRunner, sessionId);
  }

  navigate(url) {
    return this._navigate(this._testRunner.url(url));
  }

  async _navigate(url) {
    var session = await this.createSession();
    await session._navigate(url);
    await session.disconnect();
  }

  async loadHTML(html) {
    html = html.replace(/'/g, "\\'").replace(/\n/g, '\\n');
    var session = await this.createSession();
    await session.protocol.Runtime.evaluate({
      awaitPromise: true,
      expression: `
      document.write('${html}');

      // wait for all scripts to load
      const promise = new Promise(x => window._loadHTMLResolve = x).then(() => {
        delete window._loadHTMLResolve;
      });
      // We do a document.write here to serialize with the previous document.write
      if (document.querySelector('script[src]'))
        document.write('<script>window._loadHTMLResolve(); document.currentScript.remove();</script>');
      else
        window._loadHTMLResolve();

      document.close();
      promise;
    `});
    await session.disconnect();
  }
};

TestRunner.Session = class {
  constructor(testRunner, sessionId) {
    this._testRunner = testRunner;
    this._sessionId = sessionId;
    this._requestId = 0;
    this._eventHandlers = new Map();
    this.protocol = this._setupProtocol();
    this._parentSessionId = null;
    DevToolsAPI._sessions.set(sessionId, this);
  }

  async disconnect() {
    await DevToolsAPI._sendCommandOrDie(
        this._parentSessionId, 'Target.detachFromTarget',
        {sessionId: this._sessionId}, this._testRunner._protocolTimeout);
  }

  createChild(sessionId) {
    const session = new TestRunner.Session(this._testRunner, sessionId);
    session._parentSessionId = this._sessionId;
    return session;
  }

  async attachChild(targetId) {
    const {sessionId} = (await this.protocol.Target.attachToTarget({targetId, flatten: true})).result;
    return this.createChild(sessionId);
  }

  async sendCommand(method, params) {
    if (this._testRunner._dumpInspectorProtocolMessages)
      this._testRunner.log(`frontend => backend: ${JSON.stringify({method, params, sessionId: this._sessionId})}`);
    const result = await DevToolsAPI._sendCommand(
        this._sessionId, method, params, this._testRunner._protocolTimeout);
    if (this._testRunner._dumpInspectorProtocolMessages)
      this._testRunner.log(`backend => frontend: ${JSON.stringify(result)}`);
    return result;
  }

  async evaluate(code, ...args) {
    return this._innerEvaluate(false /* awaitPromise */, false /* userGesture */, code, ...args);
  }

  async evaluateAsync(code, ...args) {
    return this._innerEvaluate(true /* awaitPromise */, false /* userGesture */, code, ...args);
  }

  async evaluateAsyncWithUserGesture(code, ...args) {
    return this._innerEvaluate(true /* awaitPromise */, true /* userGesture */, code, ...args);
  }

  async _innerEvaluate(awaitPromise, userGesture, code, ...args) {
    if (typeof code === 'function') {
      var argsString = args.map(JSON.stringify.bind(JSON)).join(', ');
      code = `(${code.toString()})(${argsString})`;
    }
    var response = await this.protocol.Runtime.evaluate({expression: code, returnByValue: true, awaitPromise, userGesture});
    if (response.error) {
      const errorMessage = JSON.stringify(response.error);
      const maybeAsync = awaitPromise ? 'async ' : '';
      this._testRunner.log(`Error while evaluating ${maybeAsync}'${code}': ${errorMessage}`);
      this._testRunner.completeTest();
    } else {
      return response.result.result.value;
    }
  }

  navigate(url, waitUntil = 'load') {
    return this._navigate(this._testRunner.url(url), waitUntil);
  }

  async _navigate(url, waitUntil = 'load') {
    await this.protocol.Page.enable();
    await this.protocol.Page.setLifecycleEventsEnabled({enabled: true});
    const frameTree = await this.protocol.Page.getFrameTree();
    const frameId = frameTree.result.frameTree.frame.id;
    const navigatePromise = this.protocol.Page.navigate({url: url});
    await this.protocol.Page.onceLifecycleEvent(
        event => event.params.name === waitUntil && event.params.frameId === frameId);
    await navigatePromise;
  }

  _dispatchMessage(message) {
    if (this._testRunner._dumpInspectorProtocolMessages)
      this._testRunner.log(`backend => frontend: ${JSON.stringify(message)}`);
    var eventName = message.method;
    for (var handler of (this._eventHandlers.get(eventName) || []))
      handler(message);
  }

  /**
   * @returns {import("devtools-protocol/types/protocol-tests-proxy-api").ProtocolTestsProxyApi.ProtocolApi}
   */
  _setupProtocol() {
    return new Proxy({}, {
      get: (target, agentName, receiver) => new Proxy({}, {
        get: (target, methodName, receiver) => {
          const eventPattern = /^(on(ce)?|off)([A-Z][A-Za-z0-9]*)/;
          var match = eventPattern.exec(methodName);
          if (!match)
            return args => this.sendCommand(
                       `${agentName}.${methodName}`, args || {});
          var eventName = match[3];
          eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
          if (match[1] === 'once')
            return eventMatcher => this._waitForEvent(
                       `${agentName}.${eventName}`, eventMatcher);
          if (match[1] === 'off')
            return listener => this._removeEventHandler(
                       `${agentName}.${eventName}`, listener);
          return listener => this._addEventHandler(
                     `${agentName}.${eventName}`, listener);
        }
      })
    });
  }

  _addEventHandler(eventName, handler) {
    var handlers = this._eventHandlers.get(eventName) || [];
    handlers.push(handler);
    this._eventHandlers.set(eventName, handlers);
  }

  _removeEventHandler(eventName, handler) {
    var handlers = this._eventHandlers.get(eventName) || [];
    var index = handlers.indexOf(handler);
    if (index === -1)
      return;
    handlers.splice(index, 1);
    this._eventHandlers.set(eventName, handlers);
  }

  _waitForEvent(eventName, eventMatcher) {
    return TestRunner.wrapPromiseWithTimeout(
        new Promise(callback => {
          var handler = result => {
            if (eventMatcher && !eventMatcher(result))
              return;
            this._removeEventHandler(eventName, handler);
            callback(result);
          };
          this._addEventHandler(eventName, handler);
        }),
        this._testRunner._protocolTimeout,
        `Waiting for ${eventName} timed out`);
  }
};

// Helper class to collect information of auto attached targets and
// create `TestRunner.Session` from them.
TestRunner.ChildTargetManager = class {
  // @param {TestRunner} testRunner
  // @param {Session} session
  constructor(testRunner, session) {
    this._testRunner = testRunner;
    this._session = session;
    this._attachedTargets = [];
  }

  // @param {object|undefined} autoAttachParams
  // @return {void}
  //
  // Issues `Target.setAutoAttach` and starts collecting auto attached
  // `TargetInfo`.
  async startAutoAttach(autoAttachParams) {
    autoAttachParams = autoAttachParams ||
        {autoAttach: true, flatten: true, waitForDebuggerOnStart: false};
    this._session.protocol.Target.onAttachedToTarget(event => {
      this._attachedTargets.push(event.params);
    });
    await this._session.protocol.Target.setAutoAttach(autoAttachParams);
  }

  // @param {(TargetInfo): bool} pred
  // @return {TestRunner.Session|null}
  findAttachedSession(pred) {
    const found =
        this._attachedTargets.find(({targetInfo}) => pred(targetInfo));
    return found ? this._session.createChild(found.sessionId) : null;
  }

  // @return {TestRunner.Session|null}
  findAttachedSessionPrimaryMainFrame() {
    return this.findAttachedSession(
        targetInfo =>
            targetInfo.type === 'page' && targetInfo.subtype === undefined);
  }

  // @return {TestRunner.Session|null}
  findAttachedSessionPrerender() {
    return this.findAttachedSession(
        targetInfo =>
            targetInfo.type === 'page' && targetInfo.subtype === 'prerender');
  }
};

var DevToolsAPI = {};
DevToolsAPI._requestId = 0;
DevToolsAPI._embedderMessageId = 0;
DevToolsAPI._dispatchTable = new Map();
DevToolsAPI._sessions = new Map();
DevToolsAPI._outputElement = null;

DevToolsAPI._log = function(text) {
  if (!DevToolsAPI._outputElement) {
    var intermediate = document.createElement('div');
    document.body.appendChild(intermediate);
    var intermediate2 = document.createElement('div');
    intermediate.appendChild(intermediate2);
    DevToolsAPI._outputElement = document.createElement('div');
    DevToolsAPI._outputElement.className = 'output';
    DevToolsAPI._outputElement.id = 'output';
    DevToolsAPI._outputElement.style.whiteSpace = 'pre';
    intermediate2.appendChild(DevToolsAPI._outputElement);
  }
  DevToolsAPI._outputElement.appendChild(document.createTextNode(text));
  DevToolsAPI._outputElement.appendChild(document.createElement('br'));
};

DevToolsAPI._completeTest = function() {
  testRunner.notifyDone();
};

DevToolsAPI._die = function(message, error) {
  DevToolsAPI._log(`${message}: ${error}\n${error.stack}`);
  DevToolsAPI._completeTest();
  throw new Error();
};

DevToolsAPI.dispatchMessage = function(messageOrObject) {
  var messageObject = (typeof messageOrObject === 'string' ? JSON.parse(messageOrObject) : messageOrObject);
  var messageId = messageObject.id;
  try {
    if (typeof messageId === 'number') {
      var handler = DevToolsAPI._dispatchTable.get(messageId);
      if (handler) {
        DevToolsAPI._dispatchTable.delete(messageId);
        handler(messageObject);
      } else {
        DevToolsAPI._die(`Unexpected result id ${messageId}`);
      }
    } else {
      var session = DevToolsAPI._sessions.get(messageObject.sessionId || '');
      if (session)
        session._dispatchMessage(messageObject);
    }
  } catch(e) {
    DevToolsAPI._die(`Exception when dispatching message\n${JSON.stringify(messageObject)}`, e);
  }
};

DevToolsAPI._sendCommand = function(sessionId, method, params, timeout = 0) {
  var requestId = ++DevToolsAPI._requestId;
  var messageObject = {'id': requestId, 'method': method, 'params': params};
  if (sessionId)
    messageObject.sessionId = sessionId;
  var embedderMessage = {'id': ++DevToolsAPI._embedderMessageId, 'method': 'dispatchProtocolMessage', 'params': [JSON.stringify(messageObject)]};
  DevToolsHost.sendMessageToEmbedder(JSON.stringify(embedderMessage));
  return TestRunner.wrapPromiseWithTimeout(
      new Promise(f => DevToolsAPI._dispatchTable.set(requestId, f)), timeout,
      `${method} command timed out`);
};

DevToolsAPI._sendCommandOrDie = function(sessionId, method, params, timeout) {
  return DevToolsAPI._sendCommand(sessionId, method, params, timeout)
      .then(message => {
        if (message.error)
          DevToolsAPI._die(
              'Error communicating with harness',
              new Error(JSON.stringify(message.error)));
        return message.result;
      });
};

DevToolsAPI._fetch = function(url) {
  return new Promise(fulfill => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onreadystatechange = e => {
      if (xhr.readyState !== XMLHttpRequest.DONE)
        return;
      if ([0, 200, 304].indexOf(xhr.status) === -1)  // Testing harness file:/// results in 0.
        DevToolsAPI._die(`${xhr.status} while fetching ${url}`, new Error());
      else
        fulfill(e.target.response);
    };
    xhr.send(null);
  });
};

testRunner.dumpAsText();
testRunner.waitUntilDone();
testRunner.setPopupBlockingEnabled(false);

window.addEventListener('load', () => {
  var params = new URLSearchParams(window.location.search);
  if (!params.get('test'))
    return;

  var testScriptURL = params.get('test');
  var testBaseURL = testScriptURL.substring(0, testScriptURL.lastIndexOf('/') + 1);

  var targetPageURL = params.get('target') || params.get('test');
  var targetBaseURL = targetPageURL.substring(0, targetPageURL.lastIndexOf('/') + 1);

  DevToolsAPI._fetch(testScriptURL).then(testScript => {
    var testRunner = new TestRunner(testBaseURL, targetBaseURL, DevToolsAPI._log, DevToolsAPI._completeTest, DevToolsAPI._fetch, params);
    var testFunction = eval(`${testScript}\n//# sourceURL=${testScriptURL}`);
    if (params.get('debug')) {
      var dispatch = DevToolsAPI.dispatchMessage;
      var messages = [];
      DevToolsAPI.dispatchMessage = message => {
        if (!messages.length) {
          setTimeout(() => {
            for (var message of messages.splice(0))
              dispatch(message);
          }, 0);
        }
        messages.push(message);
      };
      testRunner.log = console.log;
      testRunner.completeTest = () => console.log('Test completed');
      window.test = () => testFunction(testRunner);
      return;
    }
    return testFunction(testRunner);
  }).catch(reason => {
    DevToolsAPI._log(`Error while executing test script: ${reason}\n${reason.stack}`);
    DevToolsAPI._completeTest();
  });
}, false);

window['onerror'] = (message, source, lineno, colno, error) => {
  DevToolsAPI._log(`${error}\n${error.stack}`);
  DevToolsAPI._completeTest();
};

window.addEventListener('unhandledrejection', e => {
  DevToolsAPI._log(`Promise rejection: ${e.reason}\n${e.reason ? e.reason.stack : ''}`);
  DevToolsAPI._completeTest();
}, false);

TestRunner.wrapPromiseWithTimeout = (promise, timeout, label) => {
  if (!timeout)
    return promise;
  let timerId;
  // For a clearer stack trace, creating the error first.
  const error = new Error(`Timed out at ${label}`);
  const timeoutPromise = new Promise(resolve => {
    timerId = setTimeout(resolve, timeout);
  });
  return Promise.race([
    promise.then(result => {
      clearTimeout(timerId);
      return result;
    }),
    timeoutPromise.then(() => Promise.reject(error))
  ]);
};

exports.TestRunner = TestRunner;
