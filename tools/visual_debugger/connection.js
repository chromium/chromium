// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps the websocket connection and allows sending messages
 */

const Connection = {

  getUrl() {
    var request_error = "";
    var server_error = "";
    var url = "";
    url = document.querySelector('#url').value;
    if (!url) {
      var http_requester = new XMLHttpRequest();
      // Sync request to avoid complexity but is poor form.
      try {
        http_requester.open("GET", location.origin + '/discover.html', false);
        http_requester.send();
      }
      catch (req_error) {
        request_error = "Visual Debugger local server is inaccessible. \n" +
          "Please launch the server with command:\n " +
          "    ./launchdebugger {app_port} {remote_port} \n" +
          " remote_port defaults to 7777 \n" +
          " corresponds to the chromium command line\n    " +
          " --remote-debugging-port=7777 \n" +
          " app_port defaults to 8777. Currently app_port=" + location.port;
      }

      if (http_requester.status != 200) {
        server_error = "Server reports error=" + http_requester.responseText;
      }
      else {
        var discover_json = JSON.parse(http_requester.responseText);
        url = discover_json.webSocketDebuggerUrl;
      }
    }
    const return_strings = [url, request_error, server_error];
    return return_strings;
  },


  startConnection() {
    const loop_interval = 3000;
    const connect_info = this.getUrl();
    if (connect_info[1] != "") {
      window.alert(connect_info[1]);
      return;
    }
    if (connect_info[2] != "") {
      window.alert(connect_info[2]);
      return;
    }
    url = connect_info[0];

    // Create WebSocket connection.
    this.socket = new WebSocket(url);
    const status = document.querySelector('#connection-status');
    const connect = document.querySelector('#connect');
    const disconnect = document.querySelector('#disconnect');

    this.next_command_id = 1;

    // Connection opened
    this.socket.addEventListener('open', (event) => {
      const message = {};
      message['method'] = 'VisualDebugger.startStream';
      this.sendMessage(message)

      connect.setAttribute('disabled', true);
      disconnect.removeAttribute('disabled');
      status.classList.remove('disconnected');
    });

    // Listen for messages
    this.socket.addEventListener('message', (event) => {
      const json = JSON.parse(event.data);
      // We now use devtool events so our frame data is packed
      // into the args of the method.
      if (json.method === "VisualDebugger.frameResponse") {
        const frame_json = json.params.frameData;
        if (frame_json.connection == "ok") {
          Filter.sendStreamFilters();
        } else if (frame_json.frame && frame_json.drawcalls) {
          processIncomingFrame(frame_json);
        }
      }
      else if (json.error) {
        window.alert("Visual Debugger could not start stream.\n " +
          "please add 'use_viz_debugger=true' to args.gn");
        console.log(json.error);
        this.socket.close();
      }
    });

    this.socket.addEventListener('close', () => {
      connect.removeAttribute('disabled');
      disconnect.setAttribute('disabled', true);
      status.classList.add('disconnected');
      // Checks if connection can be made every
      // loop_interval number of milliseconds.
      var testing = function() {
        var interval = setInterval(function() {
          if (document.getElementById('autoconnect').checked) {
            const test_connect = Connection.getUrl();
            if (test_connect[0] != "") {
              clearInterval(interval);
              Connection.startConnection();
            }
          }
        }, loop_interval);
      }
      testing();
    });

    disconnect.addEventListener('click', () => {
      const message = {};
      message['method'] = 'VisualDebugger.stopStream';
      this.sendMessage(message);
      document.getElementById('autoconnect').checked = false;
      this.socket.close();
    });
  },

  initialize() {
    const connect = document.querySelector('#connect');
    connect.addEventListener('click', () => {
      this.startConnection();
    });
  },

  sendMessage(message) {
    if (!this.socket)
      return;
    message['id'] = this.next_command_id++;
    this.socket.send(JSON.stringify(message));
  }
};