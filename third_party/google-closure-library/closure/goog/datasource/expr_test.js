// Copyright 2006 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.ds.ExprTest');
goog.setTestOnly();

const DataManager = goog.require('goog.ds.DataManager');
const Expr = goog.require('goog.ds.Expr');
const JsDataSource = goog.require('goog.ds.JsDataSource');
const testSuite = goog.require('goog.testing.testSuite');

let jsDs;
const jsObj = {
  Success: true,
  Errors: [],
  Body: {
    Contacts: [
      {Name: 'John Doe', Email: 'john@gmail.com', EmailCount: 300},
      {Name: 'Jane Doh', Email: 'jane@gmail.com'},
      {Name: 'Steve Smith', Email: 'steve@gmail.com', EmailCount: 305},
      {Name: 'John Smith', Email: 'smith@gmail.com'},
      {Name: 'Homer Simpson', Email: 'homer@gmail.com'},
      {Name: 'Bart Simpson', Email: 'bart@gmail.com'},
    ],
  },
};

testSuite({
  setUp() {
    jsDs = new JsDataSource(jsObj, 'JS', null);
    const dm = DataManager.getInstance();
    dm.addDataSource(jsDs, true);
  },

  testBasicStuff() {
    assertNotNull('Get Body', Expr.create('$JS/Body').getNode());
  },

  testArrayExpressions() {
    assertEquals(6, Expr.create('$JS/Body/Contacts/*').getNodes().getCount());
    assertEquals(
        'John Doe', Expr.create('$JS/Body/Contacts/[0]/Name').getValue());
    assertEquals(
        305, Expr.create('$JS/Body/Contacts/[2]/EmailCount').getValue());
    assertEquals(6, Expr.create('$JS/Body/Contacts/*/count()').getValue());
    assertEquals(0, Expr.create('$JS/Errors/*/count()').getValue());
  },

  testCommonExpressions() {
    assertTrue(Expr.create('.').isCurrent_);
    assertFalse(Expr.create('Bob').isCurrent_);
    assertTrue(Expr.create('*|text()').isAllChildNodes_);
    assertFalse(Expr.create('Bob').isAllChildNodes_);
    assertTrue(Expr.create('@*').isAllAttributes_);
    assertFalse(Expr.create('Bob').isAllAttributes_);
    assertTrue(Expr.create('*').isAllElements_);
    assertFalse(Expr.create('Bob').isAllElements_);
  },

  testIndexExpressions() {
    assertEquals(Expr.create('node/[5]').getNext().size_, 1);
    assertEquals(Expr.create('node/[5]').getNext().parts_[0], '[5]');
  },
});
