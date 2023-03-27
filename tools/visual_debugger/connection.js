// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps the websocket connection and allows sending messages
 */

const Connection = {

  async getUrl() {
    let url = document.querySelector("#url").value;
    if (url) {
      return [url, ""];
    }

    try {
      let response = await fetch(location.origin + "/discover.json");
      if (!response.ok) {
        return [
          "",
          `Unexpected server error=${response.status} ${response.statusText}`,
        ];
      } else {
        let discover_json = await response.json();
        if (discover_json.error) {
          // Error message from the python server
          return ["", discover_json.error];
        }

        // Success
        return [discover_json.webSocketDebuggerUrl, ""];
      }
    } catch (e) {
      request_error =
        "Visual Debugger local server is inaccessible. \n" +
        "Please launch the server with command:\n " +
        "    ./launchdebugger {app_port} {remote_port} \n" +
        " remote_port defaults to 7777 \n" +
        " corresponds to the chromium command line\n    " +
        " --remote-debugging-port=7777 \n" +
        " app_port defaults to 8777. Currently app_port=" +
        location.port;
      return ["", request_error];
    }
  },


  async startConnection() {
    const loop_interval = 3000;
    const connect_info = await this.getUrl();
    if (connect_info[1] != "") {
      window.alert(connect_info[1]);
      return;
    }
    url = connect_info[0];

    // Create WebSocket connection.
    this.socket = new WebSocket(url);

    const status = document.querySelector('#connection-status');
    const connect = document.querySelector('#connect');
    const disconnect = document.querySelector('#disconnect');

    this.next_command_id = 1;

    this.socket.addEventListener('error', (event) => {
      document.getElementById('autoconnect').checked = false;
      window.alert("Websocket could not connect.\n You may need to add: \n " +
      "--remote-allow-origins=* \n  to your chromium launch flags.");
    });


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
      let retryAfterDelay = () => {
        setTimeout(() => {
          if (!document.getElementById("autoconnect").checked) {
            // Keep this setTimeout loop alive in case the user re-checks the
            // box.
            retryAfterDelay();
            return;
          }

          console.log("Attempting autoconnect...");
          Connection.getUrl().then((test_connect) => {
            if (test_connect[0] != "") {
              Connection.startConnection();
            } else {
              // Failure, queue a retry.
              retryAfterDelay();
            }
          });
        }, loop_interval);
      };

      retryAfterDelay();
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