// Imported from:
// * http://mxr.mozilla.org/mozilla-central/source/dom/indexedDB/test/unit/test_getAll.js
// Changes:
// * added 'use strict' since some ES6 features NYI w/o it
// * function -> function*
// * this.window -> window
// * testGenerator.send() -> testGenerator.next()
// * Added deleteDatabase() step to reset storage state

'use strict';
/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = window ? window.location.pathname : "Splendid Test";
  indexedDB.deleteDatabase(name);

  const values = [ "a", "1", 1, "foo", 300, true, false, 4.5, null ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;
  let db = event.target.result;

  let objectStore = db.createObjectStore("foo", { autoIncrement: true });

  request.onsuccess = grabEventAndContinueHandler;
  request = objectStore.mozGetAll();
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 0, "No elements");

  let addedCount = 0;

  for (let i in values) {
    request = objectStore.add(values[i]);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedCount == values.length) {
        executeSoon(function() { testGenerator.next(); });
      }
    }
  }
  yield undefined;
  yield undefined;

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll();
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, values.length, "Same length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(null, 5);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 5, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  let keyRange = IDBKeyRange.bound(1, 9);

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, values.length, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, 0);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, values.length, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, null);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, values.length, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, undefined);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, values.length, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[i], "Same value");
  }

  keyRange = IDBKeyRange.bound(4, 7);

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 4, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[parseInt(i) + 3], "Same value");
  }

  // Get should take a key range also but it doesn't return an array.
  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").get(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, false, "Not an array object");
  is(event.target.result, values[3], "Correct value");

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, 2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[parseInt(i) + 3], "Same value");
  }

  keyRange = IDBKeyRange.bound(4, 7);

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, 50);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 4, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[parseInt(i) + 3], "Same value");
  }

  keyRange = IDBKeyRange.bound(4, 7);

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange, 0);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 4, "Correct length");

  keyRange = IDBKeyRange.bound(4, 7, true, true);

  request = db.transaction("foo", "readonly", {durability: "relaxed"}).objectStore("foo").mozGetAll(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], values[parseInt(i) + 4], "Same value");
  }

  finishTest();
  yield undefined;
}
