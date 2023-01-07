// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is used to execute costly repeating timers on different renderers.
// The renderers communicate using broacast channels to make sure the timers are
// unaligned. See the HTML file to know how to invoke this with the correct
// parameters.

// Try to make this a more pleasant experience by failing fast.
'use strict';

// Setup the broadcast channel to talk to the other pages.
const bc = new BroadcastChannel('broadcast_channel');
bc.onmessage = onConnect;

// Store the params passed in directly to the page.
var param_dict = {};

// Simply return a random string of the appropriate length.
function getRandomString(length) {
  var result           = '';
  var characters       =
      'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  var charactersLength = characters.length;
  for ( var i = 0; i < length; i++ ) {
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
  }
  return result;
}

// Completely change the content of the page html to trigger rendering.
function changeInnerHTMLBody() {
  document.body.innerHTML = getRandomString(1000);
}

// Kick-off the repeating timer and warn the next peer to start also.
function startRepeatingTasks(interval){

  // Find out how long this specific peer has to wait.
  var peer_id = parseInt(param_dict["peerID"]);
  var wait_length = parseInt(param_dict["interval"]) /
      parseInt(param_dict["numPeers"]) * peer_id;

  // Busy loop until we are aligned.
  var now = Date.now();
  while(now % interval != 0){
    now = Date.now();
  }

  // Sleep for the additional time. This makes sure the tasks are unaligned.
  var deadline = now + wait_length;
  while(now < deadline){
    now = Date.now();
  }

  // Start then warn peers.
  setInterval(changeInnerHTMLBody, interval);
  bc.postMessage(JSON.stringify(param_dict));
}

// When peer n-1 has started we can start ourselves.
function onConnect(event){
  var result = JSON.parse(event.data);

  // Only react to peer n-1 starting.
  if(parseInt(result["peerID"]) == parseInt(param_dict["peerID"]) - 1){

    startRepeatingTasks(parseInt(param_dict["interval"]));
    console.log(result);
  }
}

function onLoad() {
  // Get the params from the URL
  var params = window.location.search.slice(1).split("&");
  params.forEach(function(pair){
    var split = pair.split("=");
    var name = split[0];
    var value = split[1];
    param_dict[name] = value;
  });

  var interval = parseInt(param_dict["interval"]);

  // First page can't wait for anybody else to start!
  if(param_dict["peerID"] == "1"){
    startRepeatingTasks(interval);
  }
}

function cleaup() {
  bc.close();
}
