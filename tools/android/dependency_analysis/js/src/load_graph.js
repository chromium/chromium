// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For ease of development, we load the graph from this hardcoded location by
// default. This should be changed when a snapshot picker is implemented.
const LATEST_GRAPH = 'https://storage.googleapis.com/clank-dependency-graphs/latest/all.json';

// We serve our testing data on localhost:8888 as a
// fallback that will be triggered if CORS blocks the request. In production,
// the request should go through as the domain is allowed. In localhost, CORS
// will always block the request.
const LOCALHOST_GRAPH = 'http://localhost:8888/json_graph.txt';

import * as d3 from 'd3';

/**
 * Retrieve the graph to show.
 *
 * @return {Promise} Promise resolved with the graph data.
 */
async function loadGraph() {
  try {
    // First, try LATEST_GRAPH from Cloud, which should work from production but
    // fail in local development.
    const data = await d3.json(LATEST_GRAPH);
    console.log(`Loaded graph from ${LATEST_GRAPH}`);
    return data;
  } catch (e) {
    // Then try LOCALHOST_GRAPH from localhost, which should work in local
    // development.
    const data = await d3.json(LOCALHOST_GRAPH);
    console.log(`Loaded graph from ${LOCALHOST_GRAPH}`);
    return data;
  }
}

export {
  loadGraph,
};
