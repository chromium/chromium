/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dbTest');
goog.setTestOnly();

const Cursor = goog.require('goog.db.Cursor');
const DbError = goog.require('goog.db.Error');
const GoogPromise = goog.require('goog.Promise');
const IndexedDb = goog.require('goog.db.IndexedDb');
const KeyRange = goog.require('goog.db.KeyRange');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const Transaction = goog.require('goog.db.Transaction');
const asserts = goog.require('goog.testing.asserts');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const googDb = goog.require('goog.db');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');

const idbSupported = product.CHROME;
let dbName;
const dbBaseName = 'testDb';
let globalDb = null;
let dbsToClose = [];
let propertyReplacer;

const baseVersion = 1;

let dbVersion = 1;

const TransactionMode = Transaction.TransactionMode;
const EventTypes = Transaction.EventTypes;

function openDatabase() {
  return googDb.openDatabase(dbName).addCallback((db) => {
    dbsToClose.push(db);
  });
}

function incrementVersion(db, onUpgradeNeeded) {
  db.close();

  const onBlocked = (ev) => {
    fail(`Upgrade to version ${dbVersion} is blocked.`);
  };

  return googDb.openDatabase(dbName, ++dbVersion, onUpgradeNeeded, onBlocked)
      .addCallback((db) => {
        dbsToClose.push(db);
        assertEquals(dbVersion, db.getVersion());
      });
}

function addStore(db) {
  return incrementVersion(db, (ev, db, tx) => {
    db.createObjectStore('store');
  });
}

function addStoreWithIndex(db) {
  return incrementVersion(db, (ev, db, tx) => {
    const store = db.createObjectStore('store', {keyPath: 'key'});
    store.createIndex('index', 'value');
  });
}

function populateStore(values, keys, db) {
  const putTx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
  const store = putTx.objectStore('store');
  for (let i = 0; i < values.length; ++i) {
    store.put(values[i], keys[i]);
  }
  return putTx.wait();
}

function populateStoreWithObjects(values, keys, db) {
  const putTx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
  const store = putTx.objectStore('store');
  googArray.forEach(values, (value, index) => {
    store.put({'key': keys[index], 'value': value});
  });
  return putTx.wait();
}

function assertStoreValues(values, db) {
  const assertStoreTx = db.createTransaction(['store']);
  assertStoreTx.objectStore('store').getAll().addCallback((results) => {
    assertSameElements(values, results);
  });
}

/**
 * Assert the keys are as expected.
 * @param {!Array<string>} keys - The expected keys.
 * @param {!IndexedDb} db - The indexed db.
 */
function assertStoreKeyValues(keys, db) {
  const assertStoreTx = db.createTransaction(['store']);
  assertStoreTx.objectStore('store').getAllKeys().addCallback((results) => {
    assertSameElements(keys, results);
  });
}

function assertStoreObjectValues(values, db) {
  const assertStoreTx = db.createTransaction(['store']);
  assertStoreTx.objectStore('store').getAll().addCallback((results) => {
    const retrievedValues = googArray.map(results, (result) => result['value']);
    assertSameElements(values, retrievedValues);
  });
}

function assertStoreDoesntExist(db) {
  try {
    db.createTransaction(['store']);
    fail('Create transaction with a non-existent store should have failed.');
  } catch (e) {
    // expected
    assertEquals(e.getName(), DbError.ErrorName.NOT_FOUND_ERR);
  }
}

function transactionToPromise(db, trx) {
  return new GoogPromise((resolve, reject) => {
    events.listen(trx, EventTypes.ERROR, reject);
    events.listen(trx, EventTypes.COMPLETE, () => {
      resolve(db);
    });
  });
}

// Calls onRecordReady each time that a new record can be read by the
// cursor with cursor.next(). Returns a promise that resolves or rejects
// based on when the cursor is complete or errors out. If onRecordReady
// returns a promise that promises is also waited on before the returned
// promise resolves.
function forEachRecord(cursor, onRecordReady) {
  const promises = [];
  return new GoogPromise((resolve, reject) => {
           const key = events.listen(cursor, Cursor.EventType.NEW_DATA, () => {
             const result = onRecordReady();
             if (result && ('then' in result)) {
               promises.push(result);
             }
           });

           events.listenOnce(
               cursor, [Cursor.EventType.COMPLETE, Cursor.EventType.ERROR],
               (evt) => {
                 events.unlistenByKey(key);
                 if (evt.type == Cursor.EventType.COMPLETE) {
                   resolve();
                 } else {
                   reject(evt);
                 }
               });
         })
      .then(() => GoogPromise.all(promises));
}

function failOnErrorEvent(ev) {
  fail(ev.target.message);
}

testSuite({
  setUpPage() {
    propertyReplacer = new PropertyReplacer();
  },

  setUp() {
    if (!idbSupported) {
      return;
    }

    // Always use a clean database by generating a new database name.
    dbName = dbBaseName + Date.now().toString();
    globalDb = openDatabase();
  },

  tearDown() {
    for (let i = 0; i < dbsToClose.length; i++) {
      dbsToClose[i].close();
    }
    dbsToClose = [];
    propertyReplacer.reset();
  },

  testDatabaseOpened() {
    if (!idbSupported) {
      return;
    }

    assertNotNull(globalDb);
    return globalDb.branch().addCallback((db) => {
      assertTrue(db.isOpen());
    });
  },

  testOpenWithNewVersion() {
    if (!idbSupported) {
      return;
    }

    let upgradeNeeded = false;
    return globalDb.branch()
        .addCallback((db) => {
          assertEquals(baseVersion, db.getVersion());
          return incrementVersion(db, (ev, db, tx) => {
            upgradeNeeded = true;
          });
        })
        .addCallback((db) => {
          assertTrue(upgradeNeeded);
        });
  },

  testManipulateObjectStores() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback((db) => {
          assertEquals(baseVersion, db.getVersion());
          return incrementVersion(db, (ev, db, tx) => {
            db.createObjectStore('basicStore');
            db.createObjectStore('keyPathStore', {keyPath: 'keyGoesHere'});
            db.createObjectStore('autoIncrementStore', {autoIncrement: true});
          });
        })
        .addCallback((db) => {
          const storeNames = db.getObjectStoreNames();
          assertEquals(3, storeNames.length);
          assertTrue(storeNames.contains('basicStore'));
          assertTrue(storeNames.contains('keyPathStore'));
          assertTrue(storeNames.contains('autoIncrementStore'));
          return incrementVersion(db, (ev, db, tx) => {
            db.deleteObjectStore('basicStore');
          });
        })
        .addCallback((db) => {
          const storeNames = db.getObjectStoreNames();
          assertEquals(2, storeNames.length);
          assertFalse(storeNames.contains('basicStore'));
          assertTrue(storeNames.contains('keyPathStore'));
          assertTrue(storeNames.contains('autoIncrementStore'));
        });
  },

  testBadObjectStoreManipulation() {
    if (!idbSupported) {
      return;
    }

    const expectedCode = DbError.ErrorName.INVALID_STATE_ERR;
    return globalDb.branch()
        .addCallback((db) => {
          try {
            db.createObjectStore('diediedie');
            fail('Create object store outside transaction should have failed.');
          } catch (err) {
            // expected
            assertEquals(expectedCode, err.getName());
          }
        })
        .addCallback(addStore)
        .addCallback((db) => {
          try {
            db.deleteObjectStore('store');
            fail('Delete object store outside transaction should have failed.');
          } catch (err) {
            // expected
            assertEquals(expectedCode, err.getName());
          }
        })
        .addCallback((db) => {
          return incrementVersion(db, (ev, db, tx) => {
            try {
              db.deleteObjectStore('diediedie');
              fail('Delete non-existent store should have failed.');
            } catch (err) {
              // expected
              assertEquals(DbError.ErrorName.NOT_FOUND_ERR, err.getName());
            }
          });
        });
  },

  testGetNonExistentObjectStore() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      const tx = db.createTransaction(['store']);
      try {
        tx.objectStore('diediedie');
        fail('getting non-existent object store should have failed');
      } catch (err) {
        assertEquals(DbError.ErrorName.NOT_FOUND_ERR, err.getName());
      }
    });
  },

  testCreateTransaction() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      let tx = db.createTransaction(['store']);
      assertEquals(
          'mode not READ_ONLY', TransactionMode.READ_ONLY, tx.getMode());
      tx = db.createTransaction(
          ['store'], Transaction.TransactionMode.READ_WRITE);
      assertEquals(
          'mode not READ_WRITE', TransactionMode.READ_WRITE, tx.getMode());
    });
  },

  testPutRecord() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          const initialPutTx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          const putOperation = initialPutTx.objectStore('store').put(
              {key: 'initial', value: 'value1'}, 'putKey');
          putOperation.addCallback((key) => {
            assertEquals('putKey', key);
          });
          return transactionToPromise(db, initialPutTx);
        })
        .addCallback((db) => {
          const checkResultsTx = db.createTransaction(['store']);
          const getOperation =
              checkResultsTx.objectStore('store').get('putKey');
          getOperation.addCallback((result) => {
            assertEquals('initial', result.key);
            assertEquals('value1', result.value);
          });
          return transactionToPromise(db, checkResultsTx);
        })
        .addCallback((db) => {
          const overwriteTx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          const putOperation = overwriteTx.objectStore('store').put(
              {key: 'overwritten', value: 'value2'}, 'putKey');
          putOperation.addCallback((key) => {
            assertEquals('putKey', key);
          });
          return transactionToPromise(db, overwriteTx);
        })
        .addCallback((db) => {
          const checkOverwriteTx = db.createTransaction(['store']);
          checkOverwriteTx.objectStore('store').get('putKey').addCallback(
              (result) => {
                // this is guaranteed to run before the COMPLETE event fires on
                // the transaction
                assertEquals('overwritten', result.key);
                assertEquals('value2', result.value);
              });

          return transactionToPromise(db, checkOverwriteTx);
        });
  },

  testAddRecord() {
    TestCase.getActiveTestCase().promiseTimeout = 60 * 1000;  // msecs

    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          const initialAddTx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          const addOperation = initialAddTx.objectStore('store').add(
              {key: 'hi', value: 'something'}, 'stuff');
          addOperation.addCallback((key) => {
            assertEquals('stuff', key);
          });
          return transactionToPromise(db, initialAddTx);
        })
        .addCallback((db) => {
          const successfulAddTx = db.createTransaction(['store']);
          const getOperation =
              successfulAddTx.objectStore('store').get('stuff');
          getOperation.addCallback((result) => {
            assertEquals('hi', result.key);
            assertEquals('something', result.value);
          });
          return transactionToPromise(db, successfulAddTx);
        })
        .addCallback((db) => {
          const addOverwriteTx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          addOverwriteTx.objectStore('store')
              .add({key: 'bye', value: 'nothing'}, 'stuff')
              .addErrback((err) => {
                // expected
                assertEquals(DbError.ErrorName.CONSTRAINT_ERR, err.getName());
              });
          return transactionToPromise(db, addOverwriteTx)
              .then(
                  () => {
                    fail('adding existing record should not have succeeded');
                  },
                  (ev) => {
                    // expected
                    assertEquals(
                        DbError.ErrorName.CONSTRAINT_ERR, ev.target.getName());
                  });
        });
  },

  testPutRecordKeyPathStore() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  db.createObjectStore('keyStore', {keyPath: 'key'});
                }))
        .addCallback((db) => {
          const putTx =
              db.createTransaction(['keyStore'], TransactionMode.READ_WRITE);
          const putOperation = putTx.objectStore('keyStore')
                                   .put({key: 'hi', value: 'something'});
          putOperation.addCallback((key) => {
            assertEquals('hi', key);
          });
          return transactionToPromise(db, putTx);
        })
        .addCallback((db) => {
          const checkResultsTx = db.createTransaction(['keyStore']);
          checkResultsTx.objectStore('keyStore')
              .get('hi')
              .addCallback((result) => {
                assertNotUndefined(result);
                assertEquals('hi', result.key);
                assertEquals('something', result.value);
              });
          return transactionToPromise(db, checkResultsTx);
        });
  },

  testPutBadRecordKeyPathStore() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  db.createObjectStore('keyStore', {keyPath: 'key'});
                }))
        .addCallback((db) => {
          const badTx =
              db.createTransaction(['keyStore'], TransactionMode.READ_WRITE);
          return badTx.objectStore('keyStore')
              .put({key: 'diedie', value: 'anything'}, 'badKey')
              .then(
                  () => {
                    fail('inserting with explicit key should have failed');
                  },
                  (err) => {
                    // expected
                    assertEquals(DbError.ErrorName.DATA_ERR, err.getName());
                  });
        });
  },

  testPutRecordAutoIncrementStore() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  db.createObjectStore('aiStore', {autoIncrement: true});
                }))
        .addCallback((db) => {
          const tx =
              db.createTransaction(['aiStore'], TransactionMode.READ_WRITE);
          const putOperation1 = tx.objectStore('aiStore').put('1');
          const putOperation2 = tx.objectStore('aiStore').put('2');
          const putOperation3 = tx.objectStore('aiStore').put('3');
          putOperation1.addCallback((key) => {
            assertNotUndefined(key);
          });
          putOperation2.addCallback((key) => {
            assertNotUndefined(key);
          });
          putOperation3.addCallback((key) => {
            assertNotUndefined(key);
          });
          return transactionToPromise(db, tx);
        })
        .addCallback((db) => {
          const tx = db.createTransaction(['aiStore']);
          const getAllOperation = tx.objectStore('aiStore').getAll();
          return getAllOperation.addCallback((results) => {
            assertEquals(3, results.length);
            // only checking to see if the results are included because the keys
            // are not specified
            assertNotEquals(-1, results.indexOf('1'));
            assertNotEquals(-1, results.indexOf('2'));
            assertNotEquals(-1, results.indexOf('3'));
          });
        });
  },

  testPutRecordKeyPathAndAutoIncrementStore() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  db.createObjectStore(
                      'hybridStore', {keyPath: 'key', autoIncrement: true});
                }))
        .addCallback((db) => {
          const tx =
              db.createTransaction(['hybridStore'], TransactionMode.READ_WRITE);
          const putOperation =
              tx.objectStore('hybridStore').put({value: 'whatever'});
          putOperation.addCallback((key) => {
            assertNotUndefined(key);
          });
          return putOperation.addCallback(() => db);
        })
        .addCallback((db) => {
          const tx = db.createTransaction(['hybridStore']);
          return tx.objectStore('hybridStore').getAll().then((results) => {
            assertEquals(1, results.length);
            assertEquals('whatever', results[0].value);
            assertNotUndefined(results[0].key);
          });
        });
  },

  testPutIllegalRecords() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      const tx = db.createTransaction(['store'], TransactionMode.READ_WRITE);

      const promises = [];
      const badKeyFail = (keyKind) => () =>
          fail(`putting with ${keyKind} key should have failed`);
      const assertExpectedError = (err) => {
        assertEquals(DbError.ErrorName.DATA_ERR, err.getName());
      };

      promises.push(tx.objectStore('store')
                        .put('death', null)
                        .then(badKeyFail('null'), assertExpectedError));

      promises.push(tx.objectStore('store')
                        .put('death', NaN)
                        .then(badKeyFail('NaN'), assertExpectedError));

      promises.push(tx.objectStore('store')
                        .put('death', undefined)
                        .then(badKeyFail('undefined'), assertExpectedError));

      return GoogPromise.all(promises);
    });
  },

  testPutIllegalRecordsWithIndex() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback((db) => {
          const tx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          const promises = [];
          const badKeyFail = (keyKind) => () => {
            fail(`putting with ${keyKind} key should have failed`);
          };
          const assertExpectedError = (err) => {
            // expected
            assertEquals(DbError.ErrorName.DATA_ERR, err.getName());
          };

          promises.push(tx.objectStore('store')
                            .put({value: 'diediedie', key: null})
                            .then(badKeyFail('null'), assertExpectedError));

          promises.push(tx.objectStore('store')
                            .put({value: 'dietodeath', key: NaN})
                            .then(badKeyFail('NaN'), assertExpectedError));
          promises.push(
              tx.objectStore('store')
                  .put({value: 'dietodeath', key: undefined})
                  .then(badKeyFail('undefined'), assertExpectedError));

          return GoogPromise.all(promises);
        });
  },

  testDeleteRecord() {
    if (!idbSupported) {
      return;
    }

    let db;
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((openedDb) => {
          db = openedDb;
          return db.createTransaction(['store'], TransactionMode.READ_WRITE)
              .objectStore('store')
              .put({key: 'hi', value: 'something'}, 'stuff');
        })
        .addCallback(
            () => db.createTransaction(['store'], TransactionMode.READ_WRITE)
                      .objectStore('store')
                      .remove('stuff'))
        .addCallback(
            () => db.createTransaction(['store']).objectStore('store').get(
                'stuff'))
        .addCallback((result) => {
          assertUndefined(result);
        });
  },

  testDeleteRange() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3'];
    const keys = ['a', 'b', 'c'];

    const addData = goog.partial(populateStore, values, keys);
    const checkStore = goog.partial(assertStoreValues, ['1']);

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(
            (db) => db.createTransaction(['store'], TransactionMode.READ_WRITE)
                        .objectStore('store')
                        .remove(KeyRange.bound('b', 'c'))
                        .then(() => db))
        .addCallback(checkStore);
  },

  testGetAll() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3'];
    const keys = ['a', 'b', 'c'];

    const addData = goog.partial(populateStore, values, keys);
    const checkStore = goog.partial(assertStoreValues, values);

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(checkStore);
  },

  testGetAllKeys() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3'];
    const keys = ['a', 'b', 'c'];

    const addData = goog.partial(populateStore, values, keys);
    const checkStore = goog.partial(assertStoreKeyValues, keys);

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(checkStore);
  },

  testObjectStoreCursorGet() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStore, values, keys);
    let db;

    const resultValues = [];
    const resultKeys = [];
    // Open the cursor over range ['b', 'c'], move in backwards direction.
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback((theDb) => {
          db = theDb;
          const cursorTx = db.createTransaction(['store']);
          const store = cursorTx.objectStore('store');

          const cursor =
              store.openCursor(KeyRange.bound('b', 'c'), Cursor.Direction.PREV);

          const whenCursorComplete = forEachRecord(cursor, () => {
            resultValues.push(cursor.getValue());
            resultKeys.push(cursor.getKey());
            cursor.next();
          });

          return GoogPromise.all([cursorTx.wait(), whenCursorComplete]);
        })
        .addCallback(() => {
          assertArrayEquals(['3', '2'], resultValues);
          assertArrayEquals(['c', 'b'], resultKeys);
        });
  },

  testObjectStoreCursorReplace() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStore, values, keys);

    // Store should contain ['1', '2', '5', '4'] after replacement.
    const checkStore = goog.partial(assertStoreValues, ['1', '2', '5', '4']);

    // Use a bounded cursor for ('b', 'c'] to update value '3' -> '5'.
    const openCursorAndReplace = (db) => {
      const cursorTx =
          db.createTransaction(['store'], TransactionMode.READ_WRITE);
      const store = cursorTx.objectStore('store');

      const cursor = store.openCursor(KeyRange.bound('b', 'c', true));
      const whenCursorComplete = forEachRecord(cursor, () => {
        assertEquals('3', cursor.getValue());
        return cursor.update('5').addCallback(() => {
          cursor.next();
        });
      });

      return GoogPromise.all([cursorTx.wait(), whenCursorComplete])
          .then(() => db);
    };

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(openCursorAndReplace)
        .addCallback(checkStore);
  },

  testObjectStoreCursorRemove() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStore, values, keys);

    // Store should contain ['1', '2'] after removing elements.
    const checkStore = goog.partial(assertStoreValues, ['1', '2']);

    // Use a bounded cursor for ('b', ...) to remove '3', '4'.
    const openCursorAndRemove = (db) => {
      const cursorTx =
          db.createTransaction(['store'], TransactionMode.READ_WRITE);

      const store = cursorTx.objectStore('store');
      const cursor = store.openCursor(KeyRange.lowerBound('b', true));
      const whenCursorComplete =
          forEachRecord(cursor, () => cursor.remove('5').addCallback(() => {
            cursor.next();
          }));
      return GoogPromise.all([cursorTx.wait(), whenCursorComplete])
          .then((results) => db);
    };

    // Setup and execute test case.
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(openCursorAndRemove)
        .addCallback(checkStore);
  },

  testClear() {
    if (!idbSupported) {
      return;
    }

    let db;
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((theDb) => {
          db = theDb;

          const putTx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          putTx.objectStore('store').put('1', 'a');
          putTx.objectStore('store').put('2', 'b');
          putTx.objectStore('store').put('3', 'c');
          return putTx.wait();
        })
        .addCallback(
            () => db.createTransaction(['store']).objectStore('store').getAll())
        .addCallback((results) => {
          assertEquals(3, results.length);
          return db.createTransaction(['store'], TransactionMode.READ_WRITE)
              .objectStore('store')
              .clear();
        })
        .addCallback(
            () => db.createTransaction(['store']).objectStore('store').getAll())
        .addCallback((results) => {
          assertEquals(0, results.length);
        });
  },

  testCommit() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          return new Promise((resolve, reject) => {
            const commitTx =
                db.createTransaction(['store'], TransactionMode.READ_WRITE);
            const store = commitTx.objectStore('store');
            store.put('data', 'stuff');
            commitTx.commit(false /* allowNoopWhenUnsupported */);
            store.put('data', 'another stuff')
                .addCallback(() => {
                  fail('Should not able to add new data after commit');
                })
                .addErrback((e) => {
                  assertEquals(
                      DbError.ErrorName.TRANSACTION_INACTIVE_ERR, e.getName());
                });
            events.listen(commitTx, EventTypes.ERROR, reject);
            events.listen(commitTx, EventTypes.COMPLETE, () => {
              resolve(db);
            });
          });
        })
        .addCallback((db) => {
          const checkResultsTx = db.createTransaction(['store']);
          return checkResultsTx.objectStore('store').getAll();
        })
        .addCallback((result) => {
          // Only 1 entry is committed
          assertEquals(1, result.length);
          assertEquals('data', result[0]);
        });
  },

  testCommitNotSupported() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      if (!IDBTransaction.prototype.hasOwnProperty('commit')) {
        const commitTx =
            db.createTransaction(['store'], TransactionMode.READ_WRITE);
        try {
          commitTx.commit();
        } catch (e) {
          assertEquals(DbError.ErrorName.UNKNOWN_ERR, e.getName());
        }
      }
    });
  },

  testCommitTwice() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      const commitTx =
          db.createTransaction(['store'], TransactionMode.READ_WRITE);
      commitTx.commit(false /* allowNoopWhenUnsupported */);
      try {
        commitTx.commit(false /* allowNoopWhenUnsupported */);
      } catch (e) {
        assertEquals(DbError.ErrorName.INVALID_STATE_ERR, e.getName());
      }
    });
  },

  testAbortTransaction() {
    if (!idbSupported) {
      return;
    }

    let db;
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((theDb) => {
          db = theDb;
          return new Promise((resolve, reject) => {
            const abortTx =
                db.createTransaction(['store'], TransactionMode.READ_WRITE);
            abortTx.objectStore('store')
                .put('data', 'stuff')
                .addCallback(() => {
                  abortTx.abort();
                });
            events.listen(abortTx, EventTypes.ERROR, reject);

            events.listen(abortTx, EventTypes.COMPLETE, () => {
              fail(
                  'transaction shouldn\'t have' +
                  ' completed after being aborted');
            });

            events.listen(abortTx, EventTypes.ABORT, resolve);
          });
        })
        .addCallback(() => {
          const checkResultsTx = db.createTransaction(['store']);
          return checkResultsTx.objectStore('store').get('stuff');
        })
        .addCallback((result) => {
          assertUndefined(result);
        });
  },

  testInactiveTransaction() {
    if (!idbSupported) {
      return;
    }

    let db;
    let store;
    let index;
    const createAndFinishTransaction = (theDb) => {
      db = theDb;
      const tx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
      store = tx.objectStore('store');
      index = store.getIndex('index');
      store.put({key: 'something', value: 'anything'});
      return tx.wait();
    };

    const assertCantUseInactiveTransaction = () => {
      const expectedCode = DbError.ErrorName.TRANSACTION_INACTIVE_ERR;
      const promises = [];

      const failOp = (op) => () => {
        fail(`${op} with inactive transaction should have failed`);
      };
      const assertCorrectError = (err) => {
        assertEquals(expectedCode, err.getName());
      };
      const keyRange = KeyRange.bound('a', 'a');
      promises.push(store.put({key: 'another', value: 'thing'})
                        .then(failOp('putting'), assertCorrectError));
      promises.push(store.add({key: 'another', value: 'thing'})
                        .then(failOp('adding'), assertCorrectError));
      promises.push(store.remove('something')
                        .then(failOp('deleting'), assertCorrectError));
      promises.push(
          store.get('something').then(failOp('getting'), assertCorrectError));
      promises.push(
          store.getAll().then(failOp('getting all'), assertCorrectError));
      promises.push(
          store.clear().then(failOp('clearing all'), assertCorrectError));

      promises.push(
          index.get('anything')
              .then(failOp('getting from index'), assertCorrectError));
      promises.push(
          index.getKey('anything')
              .then(failOp('getting key from index'), assertCorrectError));
      promises.push(
          index.getAll('anything')
              .then(failOp('getting all from index'), assertCorrectError));
      promises.push(
          index.getAllKeys('anything')
              .then(failOp('getting all keys from index'), assertCorrectError));
      promises.push(index.getAll(keyRange).then(
          failOp('getting all from index'), assertCorrectError));
      promises.push(index.getAllKeys(keyRange).then(
          failOp('getting all from index'), assertCorrectError));
      return GoogPromise.all(promises);
    };

    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback(createAndFinishTransaction)
        .addCallback(assertCantUseInactiveTransaction);
  },

  testWrongTransactionMode() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      const tx = db.createTransaction(['store']);
      assertEquals(Transaction.TransactionMode.READ_ONLY, tx.getMode());
      const promises = [];
      promises.push(tx.objectStore('store')
                        .put('KABOOM!', 'anything')
                        .then(
                            () => {
                              fail('putting should have failed');
                            },
                            (err) => {
                              assertEquals(
                                  DbError.ErrorName.READ_ONLY_ERR,
                                  err.getName());
                            }));
      promises.push(tx.objectStore('store')
                        .add('EXPLODE!', 'die')
                        .then(
                            () => {
                              fail('adding should have failed');
                            },
                            (err) => {
                              assertEquals(
                                  DbError.ErrorName.READ_ONLY_ERR,
                                  err.getName());
                            }));
      promises.push(tx.objectStore('store')
                        .remove('no key', 'nothing')
                        .then(
                            () => {
                              fail('deleting should have failed');
                            },
                            (err) => {
                              assertEquals(
                                  DbError.ErrorName.READ_ONLY_ERR,
                                  err.getName());
                            }));
      return GoogPromise.all(promises);
    });
  },

  testManipulateIndexes() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  const store = db.createObjectStore('store');
                  store.createIndex('index', 'attr1');
                  store.createIndex('uniqueIndex', 'attr2', {unique: true});
                  store.createIndex('multirowIndex', 'attr3', {multirow: true});
                }))
        .addCallback((db) => {
          const tx = db.createTransaction(['store']);
          const store = tx.objectStore('store');
          const index = store.getIndex('index');
          const uniqueIndex = store.getIndex('uniqueIndex');
          const multirowIndex = store.getIndex('multirowIndex');
          try {
            const dies = store.getIndex('diediedie');
            fail('getting non-existent index should have failed');
          } catch (err) {
            // expected
            assertEquals(DbError.ErrorName.NOT_FOUND_ERR, err.getName());
          }

          return tx.wait();
        })
        .addCallback((db) => {
          return incrementVersion(db, (ev, db, tx) => {
            const store = tx.objectStore('store');
            store.deleteIndex('index');
            try {
              store.deleteIndex('diediedie');
              fail('deleting non-existent index should have failed');
            } catch (err) {
              // expected
              assertEquals(DbError.ErrorName.NOT_FOUND_ERR, err.getName());
            }
          });
        })
        .addCallback((db) => {
          const tx = db.createTransaction(['store']);
          const store = tx.objectStore('store');
          try {
            const index = store.getIndex('index');
            fail('getting deleted index should have failed');
          } catch (err) {
            // expected
            assertEquals(DbError.ErrorName.NOT_FOUND_ERR, err.getName());
          }
          const uniqueIndex = store.getIndex('uniqueIndex');
          const multirowIndex = store.getIndex('multirowIndex');
        });
  },

  testAddRecordWithIndex() {
    TestCase.getActiveTestCase().promiseTimeout = 60 * 1000;  // msecs

    if (!idbSupported) {
      return;
    }

    const addData = (db) => {
      const store = db.createTransaction(['store'], TransactionMode.READ_WRITE)
                        .objectStore('store');
      assertFalse(store.getIndex('index').isUnique());
      assertEquals('value', store.getIndex('index').getKeyPath());
      return store.add({key: 'someKey', value: 'lookUpThis'})
          .addCallback(() => db);
    };
    const readAndAssertAboutData = (db) => {
      const index =
          db.createTransaction(['store']).objectStore('store').getIndex(
              'index');
      const promises = [
        index.get('lookUpThis').addCallback((result) => {
          assertNotUndefined(result);
          assertEquals('someKey', result.key);
          assertEquals('lookUpThis', result.value);
        }),
        index.getKey('lookUpThis').addCallback((result) => {
          assertNotUndefined(result);
          assertEquals('someKey', result);
        }),
      ];
      return GoogPromise.all(promises).then(() => db);
    };
    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback(addData)
        .addCallback(readAndAssertAboutData);
  },

  testGetMultipleRecordsFromIndex() {
    if (!idbSupported) {
      return;
    }

    const addData = (db) => {
      const addTx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
      addTx.objectStore('store').add({key: '1', value: 'a'});
      addTx.objectStore('store').add({key: '2', value: 'a'});
      addTx.objectStore('store').add({key: '3', value: 'b'});

      return addTx.wait();
    };
    const readData = (db) => {
      const index =
          db.createTransaction(['store']).objectStore('store').getIndex(
              'index');
      const promises = [];
      const keyRange = KeyRange.bound('a', 'a');
      promises.push(index.getAll().addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(3, results.length);
      }));
      promises.push(index.getAll('a').addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(2, results.length);
      }));
      promises.push(index.getAllKeys().addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(3, results.length);
        assertArrayEquals(['1', '2', '3'], results);
      }));
      promises.push(index.getAllKeys('b').addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(1, results.length);
        assertArrayEquals(['3'], results);
      }));
      promises.push(index.getAll(keyRange).addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(2, results.length);
      }));
      promises.push(index.getAllKeys(keyRange).addCallback((results) => {
        assertNotUndefined(results);
        assertEquals(2, results.length);
        assertArrayEquals(['1', '2'], results);
      }));

      return GoogPromise.all(promises).then(() => db);
    };
    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback(addData)
        .addCallback(readData);
  },

  testUniqueIndex() {
    if (!idbSupported) {
      return;
    }

    const storeDuplicatesToUniqueIndex = (db) => {
      const tx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
      assertTrue(tx.objectStore('store').getIndex('index').isUnique());
      tx.objectStore('store').add({key: '1', value: 'a'});
      tx.objectStore('store').add({key: '2', value: 'a'});
      return transactionToPromise(db, tx).then(
          () => {
            fail('Expected transaction violating unique constraint to fail');
          },
          (ev) => {
            // expected
            assertEquals(DbError.ErrorName.CONSTRAINT_ERR, ev.target.getName());
          });
    };

    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  const store = db.createObjectStore('store', {keyPath: 'key'});
                  store.createIndex('index', 'value', {unique: true});
                }))
        .addCallback(storeDuplicatesToUniqueIndex);
  },

  testDeleteDatabase() {
    if (!idbSupported) {
      return;
    }

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          db.close();
          return googDb.deleteDatabase(dbName, () => {
            fail('didn\'t expect deleteDatabase to be blocked');
          });
        })
        .addCallback(openDatabase)
        .addCallback(assertStoreDoesntExist);
  },

  testDeleteDatabaseIsBlocked() {
    if (!idbSupported) {
      return;
    }

    let wasBlocked = false;
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          db.close();
          // Get a fresh connection, without any events registered on globalDb.
          return googDb.openDatabase(dbName);
        })
        .addCallback((db) => {
          dbsToClose.push(db);
          return googDb.deleteDatabase(dbName, (ev) => {
            wasBlocked = true;
            db.close();
          });
        })
        .addCallback(() => {
          assertTrue(wasBlocked);
          return openDatabase();
        })
        .addCallback(assertStoreDoesntExist);
  },

  testBlockedDeleteDatabaseWithVersionChangeEvent() {
    if (!idbSupported) {
      return;
    }

    let gotVersionChange = false;
    return globalDb.branch()
        .addCallback(addStore)
        .addCallback((db) => {
          db.close();
          // Get a fresh connection, without any events registered on globalDb.
          return googDb.openDatabase(dbName);
        })
        .addCallback((db) => {
          dbsToClose.push(db);
          events.listen(db, IndexedDb.EventType.VERSION_CHANGE, (ev) => {
            gotVersionChange = true;
            db.close();
          });
          return googDb.deleteDatabase(dbName);
        })
        .addCallback(() => {
          assertTrue(gotVersionChange);
          return openDatabase();
        })
        .addCallback(assertStoreDoesntExist);
  },

  testDeleteNonExistentDatabase() {
    if (!idbSupported) {
      return;
    }

    // Deleting non-existent db is a no-op.  Shall not throw anything.
    return globalDb.branch().addCallback((db) => {
      db.close();
      return googDb.deleteDatabase('non-existent-db');
    });
  },

  testObjectStoreCountAll() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStore, values, keys);

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback((db) => {
          const tx = db.createTransaction(['store']);
          return tx.objectStore('store').count().addCallback((count) => {
            assertEquals(values.length, count);
          });
        });
  },

  testObjectStoreCountSome() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStore, values, keys);
    const countData = (db) => {
      const tx = db.createTransaction(['store']);
      return tx.objectStore('store')
          .count(KeyRange.bound('b', 'c'))
          .addCallback((count) => {
            assertEquals(2, count);
          });
    };

    return globalDb.branch()
        .addCallback(addStore)
        .addCallback(addData)
        .addCallback(countData);
  },

  testIndexCursorGet() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];
    const addData = goog.partial(populateStoreWithObjects, values, keys);

    const valuesResult = [];
    const keysResult = [];

    // Open the cursor over range ['b', 'c'], move in backwards direction.
    const walkBackwardsOverCursor = (db) => {
      const cursorTx = db.createTransaction(['store']);
      const index = cursorTx.objectStore('store').getIndex('index');
      const values = [];
      const keys = [];

      const cursor =
          index.openCursor(KeyRange.bound('2', '3'), Cursor.Direction.PREV);
      const cursorFinished = forEachRecord(cursor, () => {
        valuesResult.push(cursor.getValue()['value']);
        keysResult.push(cursor.getValue()['key']);
        cursor.next();
      });

      return GoogPromise.all([cursorFinished, cursorTx.wait()]).then(() => db);
    };

    return globalDb.branch()
        .addCallbacks(addStoreWithIndex)
        .addCallback(addData)
        .addCallback(walkBackwardsOverCursor)
        .addCallback((db) => {
          assertArrayEquals(['3', '2'], valuesResult);
          assertArrayEquals(['c', 'b'], keysResult);
        });
  },

  testIndexCursorReplace() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStoreWithObjects, values, keys);
    const valuesResult = [];
    const keysResult = [];

    // Store should contain ['1', '2', '5', '4'] after replacement.
    const checkStore =
        goog.partial(assertStoreObjectValues, ['1', '2', '5', '4']);

    // Use a bounded cursor for ['3', '4') to update value '3' -> '5'.
    const openCursorAndReplace = (db) => {
      const cursorTx =
          db.createTransaction(['store'], TransactionMode.READ_WRITE);
      const index = cursorTx.objectStore('store').getIndex('index');
      const cursor = index.openCursor(KeyRange.bound('3', '4', false, true));

      const cursorFinished = forEachRecord(cursor, () => {
        assertEquals('3', cursor.getValue()['value']);
        return cursor.update({'key': cursor.getValue()['key'], 'value': '5'})
            .addCallback(() => {
              cursor.next();
            });
      });

      return GoogPromise.all([cursorFinished, cursorTx.wait()])
          .then((results) => db);
    };

    // Setup and execute test case.
    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback(addData)
        .addCallback(openCursorAndReplace)
        .addCallback(checkStore);
  },

  testIndexCursorRemove() {
    if (!idbSupported) {
      return;
    }

    const values = ['1', '2', '3', '4'];
    const keys = ['a', 'b', 'c', 'd'];

    const addData = goog.partial(populateStoreWithObjects, values, keys);

    // Store should contain ['1', '2'] after removing elements.
    const checkStore = goog.partial(assertStoreObjectValues, ['1', '2']);

    // Use a bounded cursor for ('2', ...) to remove '3', '4'.
    const openCursorAndRemove = (db) => {
      const cursorTx =
          db.createTransaction(['store'], TransactionMode.READ_WRITE);

      const store = cursorTx.objectStore('store');
      const index = store.getIndex('index');
      const cursor = index.openCursor(KeyRange.lowerBound('2', true));
      const cursorFinished =
          forEachRecord(cursor, () => cursor.remove('5').addCallback(() => {
            cursor.next();
          }));

      return GoogPromise.all([cursorFinished, cursorTx.wait()])
          .then((results) => db);
    };

    // Setup and execute test case.
    return globalDb.branch()
        .addCallback(addStoreWithIndex)
        .addCallback(addData)
        .addCallback(openCursorAndRemove)
        .addCallback(checkStore);
  },

  testCanWaitForTransactionToComplete() {
    if (!idbSupported) {
      return;
    }
    return globalDb.branch().addCallback(addStore).addCallback((db) => {
      const tx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
      tx.objectStore('store').add({key: 'hi', value: 'something'}, 'stuff');
      return tx.wait();
    });
  },

  testWaitingOnTransactionThatHasAnError() {
    if (!idbSupported) {
      return;
    }
    return globalDb.branch()
        .addCallback(
            (db) => incrementVersion(
                db,
                (ev, db, tx) => {
                  const store = db.createObjectStore('store', {keyPath: 'key'});
                  store.createIndex('index', 'value', {unique: true});
                }))
        .addCallback((db) => {
          const tx =
              db.createTransaction(['store'], TransactionMode.READ_WRITE);
          assertTrue(tx.objectStore('store').getIndex('index').isUnique());
          tx.objectStore('store').add({key: '1', value: 'a'});
          tx.objectStore('store').add({key: '2', value: 'a'});
          return transactionToPromise(db, tx).then(
              () => {
                fail('expected transaction to fail');
              },
              (ev) => {
                // expected
                assertEquals(
                    DbError.ErrorName.CONSTRAINT_ERR, ev.target.getName());
              });
        });
  },

  testWaitingOnAnAbortedTransaction() {
    if (!idbSupported) {
      return;
    }
    return globalDb.addCallback(addStore).addCallback((db) => {
      const tx = db.createTransaction(['store'], TransactionMode.READ_WRITE);
      const waiting = tx.wait().then(
          () => {
            fail('Wait result should have failed');
          },
          (e) => {
            assertEquals(DbError.ErrorName.ABORT_ERR, e.getName());
          });
      tx.abort();
      return waiting;
    });
  },
});
