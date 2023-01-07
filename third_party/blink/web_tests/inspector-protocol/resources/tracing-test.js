// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(class TracingHelper {
  constructor(testRunner, session) {
    this._testRunner = testRunner;
    this._session = session;
  }

  startTracing(categories="-*,disabled-by-default-devtools.timeline,devtools.timeline") {
    return this.startTracingWithArguments({ "categories": categories, "type": "", "options": "" });
  }

  startTracingAndSaveAsStream() {
    var args = {
      "categories": "-*,disabled-by-default-devtools.timeline,devtools.timeline",
      "type": "",
      "options": "",
      "transferMode": "ReturnAsStream"
    };
    return this.startTracingWithArguments(args);
  }

  async startTracingWithArguments(args) {
    await this._session.protocol.Tracing.start(args);
    this._testRunner.log("Recording started");
  }

  async stopTracing(filter_re=/devtools.timeline/) {
    var devtoolsEvents = [];

    function dataCollected(reply) {
      var allEvents = reply.params.value;
      var filteredEvents = allEvents.filter(e => filter_re.test(e.cat));
      devtoolsEvents = devtoolsEvents.concat(filteredEvents);
    };

    this._session.protocol.Tracing.onDataCollected(dataCollected);
    this._session.protocol.Tracing.end();
    await this._session.protocol.Tracing.onceTracingComplete();
    this._testRunner.log("Tracing complete");
    this._session.protocol.Tracing.offDataCollected(dataCollected);
    this._devtoolsEvents = devtoolsEvents;
    return devtoolsEvents;
  }

  async stopTracingAndReturnStream() {
    function dataCollected() {
      this._testRunner.log("FAIL: dataCollected event should not be fired when returning trace as stream.");
    }

    this._session.protocol.Tracing.onDataCollected(dataCollected);
    this._session.protocol.Tracing.end();
    var event = await this._session.protocol.Tracing.onceTracingComplete();
    this._testRunner.log("Tracing complete");
    this._session.protocol.Tracing.offDataCollected(dataCollected);
    return event.params.stream;
  }

  retrieveStream(streamHandle, offset, chunkSize) {
    var callback;
    var promise = new Promise(f => callback = f);
    var result = "";
    var had_eof = false;

    var readArguments = { handle: streamHandle };
    if (typeof chunkSize === "number")
      readArguments.size = chunkSize;
    var firstReadArguments = JSON.parse(JSON.stringify(readArguments));
    if (typeof offset === "number")
      firstReadArguments.offset = offset;
    this._session.protocol.IO.read(firstReadArguments).then(message => onChunkRead.call(this, message.result));
    // Assure multiple in-flight reads are fine (also, save on latencies).
    this._session.protocol.IO.read(readArguments).then(message => onChunkRead.call(this, message.result));
    return promise;

    function onChunkRead(response) {
      if (had_eof)
        return;
      result += response.data;
      if (response.eof) {
        // Ignore stray callbacks from proactive read requests.
        had_eof = true;
        if (response.base64Encoded)
          result = atob(result);
        callback(result);
        return;
      }
      this._session.protocol.IO.read(readArguments).then(message => onChunkRead.call(this, message.result));
    }
  }

  findEvents(name, ph, condition) {
    return this._devtoolsEvents.filter(e => e.name === name && e.ph === ph && (!condition || condition(e)));
  }

  findEvent(name, ph, condition) {
    var events = this.findEvents(name, ph, condition);
    if (events.length)
      return events[0];
    throw new Error("Couldn't find event " + name + " / " + ph + "\n\n in " + JSON.stringify(this._devtoolsEvents, null, 2));
  }

  filterEvents(callback) {
    return this._devtoolsEvents.filter(callback);
  }

  async invokeAsyncWithTracing(performActions) {
    await this.startTracing();
    var data = await this._session.evaluateAsync(`(${performActions.toString()})()`);
    await this.stopTracing();
    return data;
  }

  formattedEvents() {
    var formattedEvents = this._devtoolsEvents.map(e => e.name + (e.args.data ? '(' + e.args.data.type + ')' : ''));
    return JSON.stringify(formattedEvents, null, 2);
  }
})
