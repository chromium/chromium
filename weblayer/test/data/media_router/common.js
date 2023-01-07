/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @fileoverview Test utilities for presentation integration tests.
 */

var startPresentationPromise = null;
var startedConnection = null;
var reconnectedConnection = null;
var presentationUrl = "cast:CCCCCCCC";
let params = (new URL(window.location.href)).searchParams;

var presentationRequest = new PresentationRequest([presentationUrl]);
var defaultRequestConnectionId = null;
var lastExecutionResult = null;

window.navigator.presentation.defaultRequest = presentationRequest;
window.navigator.presentation.defaultRequest.onconnectionavailable = function(
    e) {
  defaultRequestConnectionId = e.connection.id;
};

/**
 * Waits until one sink is available.
 */
function waitUntilDeviceAvailable() {
  presentationRequest.getAvailability(presentationUrl)
      .then(function(availability) {
        console.log('availability ' + availability.value + '\n');
        if (availability.value) {
          sendResult(true, '');
        } else {
          availability.onchange = function(newAvailability) {
            if (newAvailability)
              sendResult(true, '');
          }
        }
      })
      .catch(function(e) {
        sendResult(false, 'got error: ' + e);
      });
}

/**
 * Starts presentation.
 */
function startPresentation() {
  startPresentationPromise = presentationRequest.start();
  console.log('start presentation');
  sendResult(true, '');
}

/**
 * Checks if the presentation has been started successfully.
 */
function checkConnection() {
  if (!startPresentationPromise) {
    sendResult(false, 'Did not attempt to start presentation');
  } else {
    startPresentationPromise
        .then(function(connection) {
          if (!connection) {
            sendResult(
                false, 'Failed to start presentation: connection is null');
          } else {
            // set the new connection
            startedConnection = connection;
            waitForConnectedStateAndSendResult(startedConnection);
          }
        })
        .catch(function(e) {
          // terminate old connection if exists
          startedConnection && startedConnection.terminate();
          sendResult(
              false, 'Failed to start connection: encountered exception ' + e);
        })
  }
}

/**
 * Asserts the current state of the connection is 'connected' or 'connecting'.
 * If the current state is connecting, waits for it to become 'connected'.
 * @param {!PresentationConnection} connection
 */
function waitForConnectedStateAndSendResult(connection) {
  console.log('connection state is "' + connection.state + '"');
  if (connection.state == 'connected') {
    sendResult(true, '');
  } else if (connection.state == 'connecting') {
    connection.onconnect = () => {
      sendResult(true, '');
    };
  } else {
    sendResult(
        false,
        'Expect connection state to be "connecting" or "connected", actual: ' +
            connection.state);
  }
}

/**
 * Checks the start() request fails with expected error and message substring.
 * @param {!string} expectedErrorName
 * @param {!string} expectedErrorMessageSubstring
 */
function checkStartFailed(expectedErrorName, expectedErrorMessageSubstring) {
  if (!startPresentationPromise) {
    sendResult(false, 'Did not attempt to start presentation');
  } else {
    startPresentationPromise
        .then(function(connection) {
          sendResult(false, 'start() unexpectedly succeeded.');
        })
        .catch(function(e) {
          if (expectedErrorName != e.name) {
            sendResult(
                false, 'Got unexpected error. ' + e.name + ': ' + e.message);
          } else if (e.message.indexOf(expectedErrorMessageSubstring) == -1) {
            sendResult(
                false,
                'Error message is not correct, it should contain "' +
                    expectedErrorMessageSubstring + '"');
          } else {
            sendResult(true, '');
          }
        })
  }
}

/**
 * Terminates current presentation.
 */
function terminateConnectionAndWaitForStateChange() {
  if (startedConnection) {
    startedConnection.onterminate = function() {
      sendResult(true, '');
    };
    startedConnection.terminate();
  } else {
    sendResult(false, 'startedConnection does not exist.');
  }
}

/**
 * Closes |startedConnection| and waits for its onclose event.
 */
function closeConnectionAndWaitForStateChange() {
  if (startedConnection) {
    if (startedConnection.state == 'closed') {
      sendResult(false, 'startedConnection is unexpectedly closed.');
      return;
    }
    startedConnection.onclose = function() {
      sendResult(true, '');
    };
    startedConnection.close();
  } else {
    sendResult(false, 'startedConnection does not exist.');
  }
}

/**
 * Sends a message to |startedConnection| and expects InvalidStateError to be
 * thrown. Requires |startedConnection.state| to not equal |initialState|.
 */
function checkSendMessageFailed(initialState) {
  if (!startedConnection) {
    sendResult(false, 'startedConnection does not exist.');
    return;
  }
  if (startedConnection.state != initialState) {
    sendResult(
        false,
        'startedConnection.state is "' + startedConnection.state +
            '", but we expected "' + initialState + '".');
    return;
  }

  try {
    startedConnection.send('test message');
  } catch (e) {
    if (e.name == 'InvalidStateError') {
      sendResult(true, '');
    } else {
      sendResult(false, 'Got an unexpected error: ' + e.name);
    }
  }
  sendResult(false, 'Expected InvalidStateError but it was never thrown.');
}

/**
 * Sends a message, and expects the connection to close on error.
 */
function sendMessageAndExpectConnectionCloseOnError() {
  if (!startedConnection) {
    sendResult(false, 'startedConnection does not exist.');
    return;
  }
  startedConnection.onclose = function(event) {
    var reason = event.reason;
    if (reason != 'error') {
      sendResult(false, 'Unexpected close reason: ' + reason);
      return;
    }
    sendResult(true, '');
  };
  startedConnection.send('foo');
}

/**
 * Sends the given message, and expects response from the receiver.
 * @param {!string} message
 */
function sendMessageAndExpectResponse(message) {
  if (!startedConnection) {
    sendResult(false, 'startedConnection does not exist.');
    return;
  }
  if (startedConnection.state != 'connected') {
    sendResult(
        false,
        'Expected the connection state to be connected but it was ' +
            startedConnection.state);
    return;
  }
  startedConnection.onmessage = function(receivedMessage) {
    var expectedResponse = 'Pong: ' + message;
    var actualResponse = receivedMessage.data;
    if (actualResponse != expectedResponse) {
      sendResult(
          false,
          'Expected message: ' + expectedResponse +
              ', but got: ' + actualResponse);
      return;
    }
    sendResult(true, '');
  };
  startedConnection.send(message);
}

/**
 * Sends 'close' to receiver page, and expects receiver page closing
 * the connection.
 */
function initiateCloseFromReceiverPage() {
  if (!startedConnection) {
    sendResult(false, 'startedConnection does not exist.');
    return;
  }
  if (startedConnection.state != 'connected') {
    sendResult(
        false,
        'Expected the connection state to be connected but it was ' +
            startedConnection.state);
    return;
  }
  startedConnection.onclose = (event) => {
    const reason = event.reason;
    if (reason != 'closed') {
      sendResult(false, 'Unexpected close reason: ' + reason);
      return;
    }
    sendResult(true, '');
  };
  startedConnection.send('close');
}

/**
 * Reconnects to |connectionId| and verifies that it succeeds.
 * @param {!string} connectionId ID of connection to reconnect.
 */
function reconnectConnection(connectionId) {
  var reconnectConnectionRequest = new PresentationRequest(presentationUrl);
  reconnectConnectionRequest.reconnect(connectionId)
      .then(function(connection) {
        if (!connection) {
          sendResult(false, 'reconnectConnection returned null connection');
        } else {
          reconnectedConnection = connection;
          waitForConnectedStateAndSendResult(reconnectedConnection);
        }
      })
      .catch(function(error) {
        sendResult(false, 'reconnectConnection failed: ' + error.message);
      });
}

/**
 * Calls reconnect(connectionId) and verifies that it fails.
 * @param {!string} connectionId ID of connection to reconnect.
 */
function reconnectConnectionAndExpectFailure(
    connectionId) {
  var reconnectConnectionRequest = new PresentationRequest(presentationUrl);
  var expectedErrorMessage = 'Unknown route';
  reconnectConnectionRequest.reconnect(connectionId)
      .then(function(connection) {
        sendResult(false, 'reconnect() unexpectedly succeeded.');
      })
      .catch(function(error) {
        if (error.message.indexOf(expectedErrorMessage) > -1) {
          sendResult(true, '');
        } else {
          sendResult(
              false,
              'Error message mismatch. Expected: ' + expectedErrorMessage +
                  ', actual: ' + error.message);
        }
      });
}

/**
 * Sends the test result back to browser test.
 * @param passed true if test passes, otherwise false.
 * @param errorMessage empty string if test passes, error message if test
 *                      fails.
 */
function sendResult(passed, errorMessage) {
  lastExecutionResult = passed ? 'passed' : errorMessage;
}
