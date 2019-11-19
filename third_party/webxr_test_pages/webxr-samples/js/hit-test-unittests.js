// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import {hitTest, filterHitTestResults} from './hit-test.js';

const hittest_json = `
[
  {
    "polygon": [
      {
        "x": -0.7949525117874146,
        "y": 0,
        "z": 0.09085708856582642,
        "w": 1
      },
      {
        "x": -0.7388008236885071,
        "y": 0,
        "z": 0.2163567692041397,
        "w": 1
      },
      {
        "x": -0.7257306575775146,
        "y": 0,
        "z": 0.24235095083713531,
        "w": 1
      },
      {
        "x": -0.6986311078071594,
        "y": 0,
        "z": 0.2608765661716461,
        "w": 1
      },
      {
        "x": -0.18674542009830475,
        "y": 0,
        "z": 0.49622973799705505,
        "w": 1
      },
      {
        "x": -0.059895534068346024,
        "y": 0,
        "z": 0.5397075414657593,
        "w": 1
      },
      {
        "x": 0.0341365709900856,
        "y": 0,
        "z": 0.5527538657188416,
        "w": 1
      },
      {
        "x": 0.28332310914993286,
        "y": 0,
        "z": 0.5463775992393494,
        "w": 1
      },
      {
        "x": 0.7734386324882507,
        "y": 0,
        "z": 0.5031757950782776,
        "w": 1
      },
      {
        "x": 0.8223313689231873,
        "y": 0,
        "z": 0.25600069761276245,
        "w": 1
      },
      {
        "x": 0.7012030482292175,
        "y": 0,
        "z": -0.04883747920393944,
        "w": 1
      },
      {
        "x": 0.4847647547721863,
        "y": 0,
        "z": -0.3673478960990906,
        "w": 1
      },
      {
        "x": 0.4113020598888397,
        "y": 0,
        "z": -0.4683343172073364,
        "w": 1
      },
      {
        "x": 0.30164092779159546,
        "y": 0,
        "z": -0.5527538657188416,
        "w": 1
      },
      {
        "x": -0.2830299437046051,
        "y": 0,
        "z": -0.5527538657188416,
        "w": 1
      },
      {
        "x": -0.6636397242546082,
        "y": 0,
        "z": -0.3377974033355713,
        "w": 1
      },
      {
        "x": -0.7396656274795532,
        "y": 0,
        "z": -0.285207599401474,
        "w": 1
      },
      {
        "x": -0.7742070555686951,
        "y": 0,
        "z": -0.1881212741136551,
        "w": 1
      },
      {
        "x": -0.8223313689231873,
        "y": 0,
        "z": 0.01371713262051344,
        "w": 1
      },
      {
        "x": -0.8136139512062073,
        "y": 0,
        "z": 0.03997987508773804,
        "w": 1
      },
      {
        "x": -0.8045119643211365,
        "y": 0,
        "z": 0.06563511490821838,
        "w": 1
      }
    ],
    "pose": {
      "0": 0.3692772388458252,
      "1": 0,
      "2": -0.9293193221092224,
      "3": 0,
      "4": 0,
      "5": 1,
      "6": 0,
      "7": 0,
      "8": 0.9293193221092224,
      "9": 0,
      "10": 0.3692772388458252,
      "11": 0,
      "12": 0.28132984042167664,
      "13": -1.115093469619751,
      "14": -1.0961706638336182,
      "15": 1
    }
  }
]`;

const ray_json = `{
  "origin": {
    "x": 0.03104975074529648,
    "y": -0.02061435580253601,
    "z": -0.06608150154352188,
    "w": 1
  },
  "direction": {
    "x": -0.22517916560173035,
    "y": -0.6192044019699097,
    "z": -0.7522501945495605,
    "w": 0
  }
}`;

function make_point(array) {
  return { x : array[0], y : array[1], z : array[2], w : array[3] };
}


function run_test(points_array, point, results_callback) {
  const polygon = points_array.map(make_point);
  const point_on_plane = make_point(point);

  const result = filterHitTestResults(
    [
      {
        plane : { polygon : polygon },
        point_on_plane : point_on_plane
      }
    ]
  );

  results_callback(result);
}

function test1() {
  console.info("-------------------- running test1      -------------------- ");

  let polygon = [
    [-1, 0, -1, 1],
    [-1, 0,  1, 1],
    [ 1, 0,  1, 1],
    [ 1, 0, -1, 1],
  ];

  run_test(polygon, [0,0,0,1], result => {
    if(result.length != 1) {
      console.error("Expected one result!");
      debugger;
    }
  });

  run_test(polygon, [2,0,0,1], result => {
    if(result.length != 0) {
      console.error("Expected no results!");
      debugger;
    }
  });

  run_test(polygon, [1.01,0,0,1], result => {
    if(result.length != 0) {
      console.error("Expected no results!");
      debugger;
    }
  });

  console.info("-------------------- test1 finished     -------------------- ");
}

class MockedPlane {
  constructor(pose, polygon) {
    this._pose = pose;
    this._polygon = polygon;
  }

  getPose(frame_of_reference_ignored) {
    return { transform : { matrix : this._pose } };
  }

  get polygon() {
    return this._polygon;
  }
}

function test2() {
  let planes_raw = JSON.parse(hittest_json);
  let ray_raw = JSON.parse(ray_json);

  let planes = planes_raw.map(plane_raw => new MockedPlane(plane_raw.pose, plane_raw.polygon));
  let ray = new XRRay(ray_raw.origin, ray_raw.direction);

  const result = hitTest(ray, planes, null);
  const result_filtered = filterHitTestResults(result);
  if(result_filtered.length != 0) {
    console.error("Expected no results!");
    debugger;
  }
}

export function runUnitTests() {
  console.info("-------------------- running unittests  -------------------- ");

  test1();
  //test2();

  console.info("-------------------- run finished       -------------------- ");
}
