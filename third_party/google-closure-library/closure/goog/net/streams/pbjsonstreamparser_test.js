/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.PbJsonStreamParserTest');
goog.setTestOnly('goog.net.streams.PbJsonStreamParserTest');

const PbJsonStreamParser = goog.require('goog.net.streams.PbJsonStreamParser');
const object = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');


/**
 * @param {!Array<!Object>} result The result to check
 * @param {number} numMessages The expected number of messages
 */
function assertMessages(result, numMessages) {
  assertEquals(numMessages, result.length);
  for (let i = 0; i < numMessages; i++) {
    assertElementsEquals(['1'], object.getKeys(result[i]));
  }
}


/**
 * @param {!Array<!Object>} result The result to check
 * @param {number} numMessages The expected number of messages
 */
function assertMessagesAndStatus(result, numMessages) {
  assertEquals(numMessages, result.length - 1);
  for (let i = 0; i < numMessages; i++) {
    assertElementsEquals(['1'], object.getKeys(result[i]));
  }
  assertElementsEquals(['2'], object.getKeys(result[result.length - 1]));
}


testSuite({
  testEmptyStream: function() {
    const parser = new PbJsonStreamParser();
    assertNull(parser.parse(' [ ] '));
  },

  testSingleMessage: /**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
      function() {
        const parser = new PbJsonStreamParser();
        const result = parser.parse('[  [[null,1,2,null, "a,b[]]]"]]  ]');
        assertMessages(result, 1);
        assertEquals('[null,1,2,null, "a,b[]]]"]', result[0][1]);
      },

  testMultipleMessages: /**
                           @suppress {checkTypes} suppression added to enable
                           type checking
                         */
      function() {
        const parser = new PbJsonStreamParser();
        const msgs = '[[1,2]  ,  [3,4],[{"a": "xyz"}]]';
        const result = parser.parse('[' + msgs + ']');
        assertMessages(result, 3);
        assertEquals('[1,2]', result[0][1]);
        assertEquals('  [3,4]', result[1][1]);
        assertEquals('[{"a": "xyz"}]', result[2][1]);
      },

  testMultipleMessagesInChunks: /**
                                   @suppress {checkTypes} suppression added to
                                   enable type checking
                                 */
      function() {
        const parser = new PbJsonStreamParser();
        const input1 = '[[[1,2]';
        const input2 = '  ,  [3,4';
        const input3 = '],[{"a": "xyz"}]]]';

        let result = parser.parse(input1);
        assertMessages(result, 1);
        assertEquals('[1,2]', result[0][1]);

        result = parser.parse(input2);
        assertNull(result);

        result = parser.parse(input3);
        assertMessages(result, 2);
        assertEquals('  [3,4]', result[0][1]);
        assertEquals('[{"a": "xyz"}]', result[1][1]);
      },

  testSingleMessageInChunks: /**
                                @suppress {checkTypes} suppression added to
                                enable type checking
                              */
      function() {
        const parser = new PbJsonStreamParser();
        const input1 = '[[[1,"really long string broken  \n    ';
        const input2 =
            '     into chunks with some whitespace in the middle"]]]';

        let result = parser.parse(input1);
        assertNull(result);

        result = parser.parse(input2);
        assertMessages(result, 1);
        assertEquals(
            '[1,"really long string broken  \n    ' +
                '     into chunks with some whitespace in the middle"]',
            result[0][1]);
      },

  testOnlyStatus: /**
                     @suppress {checkTypes} suppression added to enable type
                     checking
                   */
      function() {
        const parser = new PbJsonStreamParser();
        const status = '[1,null,"abced",[true,false]]';
        const result = parser.parse('[null,' + status + ']');
        assertMessagesAndStatus(result, 0);
        assertEquals(status, result[0][2]);
      },

  testMessagesAndStatus: /**
                            @suppress {checkTypes} suppression added to enable
                            type checking
                          */
      function() {
        const parser = new PbJsonStreamParser();
        const msgs = '[[1, null, 2], ["a", true],[]]';
        const status = '["400", "error", "bad request", {"details": null}]';
        const result = parser.parse('[' + msgs + ',' + status + ']');
        assertMessagesAndStatus(result, 3);
        assertEquals('[1, null, 2]', result[0][1]);
        assertEquals(' ["a", true]', result[1][1]);
        assertEquals('[]', result[2][1]);
        assertEquals(
            '["400", "error", "bad request", {"details": null}]', result[3][2]);
      },

  testMessagesAndStatusInChunks: /**
                                    @suppress {checkTypes} suppression added to
                                    enable type checking
                                  */
      function() {
        const parser = new PbJsonStreamParser();
        const input1 = '[[[1, null, 2], ["a", ';
        const input2 = 'true]], [';
        const input3 = '"error"]';
        const input4 = ']';

        let result = parser.parse(input1);
        assertMessages(result, 1);
        assertEquals('[1, null, 2]', result[0][1]);

        result = parser.parse(input2);
        assertMessages(result, 1);
        assertEquals(' ["a", true]', result[0][1]);

        result = parser.parse(input3);
        assertMessagesAndStatus(result, 0);
        assertEquals('["error"]', result[0][2]);

        result = parser.parse(input4);
        assertNull(result);
      },

  testInvalidInputs: /**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
      function() {
        const parser1 = new PbJsonStreamParser();
        // Invalid JSON
        assertThrows(function() {
          parser1.parse('[[["a":"b"]]]');
        });
        // Stream already broken
        assertThrows(function() {
          parser1.parse('[');
        });

        const parser2 = new PbJsonStreamParser();
        parser2.parse('[ [[1, 2]], ["error"] ]');
        // Extra input
        assertThrows(function() {
          parser2.parse(',');
        });

        const parser3 = new PbJsonStreamParser();
        // Extra element of the wrapping array
        assertThrows(function() {
          parser3.parse('[ [[1, 2]], ["error"], ["error"] ]');
        });

        const parser4 = new PbJsonStreamParser();
        // Extra element of the wrapping array in chunks
        const result = parser4.parse('[ [[1, 2]], ["error"]');
        assertMessagesAndStatus(result, 1);
        assertEquals('[1, 2]', result[0][1]);
        assertEquals('["error"]', result[1][2]);
        assertThrows(/**
                        @suppress {undefinedVars} suppression
                        added to enable type checking
                      */
                     function() {
                       parse4.parse(', ["error"]');
                     });
      },
});
