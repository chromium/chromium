// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO
//   - spacial partitioning of the data so that we don't have to scan the
//     entire scene every time we render.
//   - properly clip the SVG elements when they render, right now we are just
//     letting them go negative or off the screen.  This might give us a little
//     bit better performance?
//   - make the lines for thread creation work again.  Figure out a better UI
//     than these lines, because they can be a bit distracting.
//   - Implement filters, so that you can filter on specific event types, etc.
//   - Make the callstack box collapsable or scrollable or something, it takes
//     up a lot of screen realestate now.
//   - Figure out better ways to preserve screen realestate.
//   - Make the thread bar heights configurable, figure out a better way to
//     handle overlapping events (the pushdown code).
//   - "Sticky" info, so you can click on something, and it will stay.  Now
//     if you need to scroll the page you usually lose the info because you
//     will mouse over something else on your way to scrolling.
//   - Help / legend
//   - Loading indicator / debug console.
//   - Better colors.
//
// Dean McNamee <deanm@chromium.org>

// XML namespaces.
var svgNS = 'http://www.w3.org/2000/svg';
var xhtmlNS = 'http://www.w3.org/1999/xhtml';

function toHex(num) {
  var str = "";
  var table = "0123456789abcdef";
  for (var i = 0; i < 8; ++i) {
    str = table.charAt(num & 0xf) + str;
    num >>= 4;
  }
  return str;
}

// a TLThread represents information about a thread in the traceline data.
// A thread has a list of all events that happened on that thread, the start
// and end time of the thread, the thread id, and name, etc.
function TLThread(id, startms, endms) {
  this.id = id;
  // Default the name to the thread id, but if the application uses
  // thread naming, we might see a THREADNAME event later and update.
  this.name = "thread_" + id;
  this.startms = startms;
  this.endms = endms;
  this.events = [ ];
};

TLThread.prototype.duration_ms =
function() {
  return this.endms - this.startms;
};

TLThread.prototype.AddEvent =
function(e) {
  this.events.push(e);
};

TLThread.prototype.toString =
function() {
  var res = "TLThread -- id: " + this.id + " name: " + this.name +
            " startms: " + this.startms + " endms: " + this.endms +
            " parent: " + this.parent;
  return res;
};

// A TLEvent represents a single logged event that happened on a thread.
function TLEvent(e) {
  this.eventtype = e['eventtype'];
  this.thread = toHex(e['thread']);
  this.cpu = toHex(e['cpu']);
  this.ms = e['ms'];
  this.done = e['done'];
  this.e = e;
}

function HTMLEscape(str) {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

TLEvent.prototype.toString =
function() {
  var res = "<b>ms:</b> " + this.ms + " " +
            "<b>event:</b> " + this.eventtype + " " +
            "<b>thread:</b> " + this.thread + " " +
            "<b>cpu:</b> " + this.cpu + "<br/>";
  if ('ldrinfo' in this.e) {
    res += "<b>ldrinfo:</b> " + this.e['ldrinfo'] + "<br/>";
  }
  if ('done' in this.e && this.e['done'] > 0) {
    res += "<b>done:</b> " + this.e['done'] + " ";
    res += "<b>duration:</b> " + (this.e['done'] - this.ms) + "<br/>";
  }
  if ('syscall' in this.e) {
    res += "<b>syscall:</b> " + this.e['syscall'];
    if ('syscallname' in this.e) {
      res += " <b>syscallname:</b> " + this.e['syscallname'];
    }
    if ('retval' in this.e) {
      res += " <b>retval:</b> " + this.e['retval'];
    }
    res += "<br/>"
  }
  if ('func_addr' in this.e) {
    res += "<b>func_addr:</b> " + toHex(this.e['func_addr']);
    if ('func_addr_name' in this.e) {
      res += " <b>func_addr_name:</b> " + HTMLEscape(this.e['func_addr_name']);
    }
    res += "<br/>"
  }
  if ('stacktrace' in this.e) {
    var stack = this.e['stacktrace'];
    res += "<b>stacktrace:</b><br/>";
    for (var i = 0; i < stack.length; ++i) {
      res += "0x" + toHex(stack[i][0]) + " - " +
             HTMLEscape(stack[i][1]) + "<br/>";
    }
  }

  return res;
}

// The trace logger dumps all log events to a simple JSON array.  We delay
// and background load the JSON, since it can be large.  When the JSON is
// loaded, parseEvents(...) is called and passed the JSON data.  To make
// things easier, we do a few passes on the data to group them together by
// thread, gather together some useful pieces of data in a single place,
// and form more of a structure out of the data.  We also build links
// between related events, for example a thread creating a new thread, and
// the new thread starting to run.  This structure is fairly close to what
// we want to represent in the interface.

// Delay load the JSON data.  We want to display the order in the order it was
// passed to us.  Since we have no way of correlating the json callback to
// which script element it was called on, we load them one at a time.

function JSONLoader(json_urls) {
  this.urls_to_load = json_urls;
  this.script_element = null;
}

JSONLoader.prototype.IsFinishedLoading =
function() { return this.urls_to_load.length == 0; };

// Start loading of the next JSON URL.
JSONLoader.prototype.LoadNext =
function() {
  var sc = document.createElementNS(
      'http://www.w3.org/1999/xhtml', 'script');
  this.script_element = sc;

  sc.setAttribute("src", this.urls_to_load[0]);
  document.getElementsByTagNameNS(xhtmlNS, 'body')[0].appendChild(sc);
};

// Callback counterpart to load_next, should be called when the script element
// is finished loading.  Returns the URL that was just loaded.
JSONLoader.prototype.DoneLoading =
function() {
  // Remove the script element from the DOM.
  this.script_element.parentNode.removeChild(this.script_element);
  this.script_element = null;
  // Return the URL that had just finished loading.
  return this.urls_to_load.shift();
};

var loader = null;

function loadJSON(json_urls) {
  loader = new JSONLoader(json_urls);
  if (!loader.IsFinishedLoading())
    loader.LoadNext();
}

var traceline = new Traceline();

// Called from the JSON with the log event array.
function parseEvents(json) {
  loader.DoneLoading();

  var done = loader.IsFinishedLoading();
  if (!done)
    loader.LoadNext();

  traceline.ProcessJSON(json);

  if (done)
    traceline.Render();
}

// The Traceline class represents our entire state, all of the threads from
// all sets of data, all of the events, DOM elements, etc.
function Traceline() {
  // The array of threads that existed in the program.  Hopefully in order
  // they were created.  This includes all threads from all sets of data.
  this.threads = [ ];

  // Keep a mapping of where in the list of threads a set starts...
  this.thread_set_indexes = [ ];

  // Map a thread id to the index in the threads array.  A thread ID is the
  // unique ID from the OS, along with our set id of which data file we were.
  this.threads_by_id = { };

  // The last event time of all of our events.
  this.endms = 0;

  // Constants for SVG rendering...
  this.kThreadHeightPx = 16;
  this.kTimelineWidthPx = 1008;
}

// Called to add another set of data into the traceline.
Traceline.prototype.ProcessJSON =
function(json_data) {
  // Keep track of which threads belong to which sets of data...
  var set_id = this.thread_set_indexes.length;
  this.thread_set_indexes.push(this.threads.length);

  // TODO make this less hacky.  Used to connect related events, like creating
  // a thread and then having that thread run (two separate events which are
  // related but come in at different times, etc).
  var tiez = { };

  // Run over the data, building TLThread's and TLEvents, and doing some
  // processing to put things in an easier to display form...
  for (var i = 0, il = json_data.length; i < il; ++i) {
    var e = new TLEvent(json_data[i]);

    // Create a unique identifier for a thread by using the id of this data
    // set, so that they are isolated from other sets of data with the same
    // thread id, etc.  TODO don't overwrite the original...
    e.thread = set_id + '_' + e.thread;

    // If this is the first event ever seen on this thread, create a new
    // thread object and add it to our lists of threads.
    if (!(e.thread in this.threads_by_id)) {
      var end_ms = e.done ? e.done : e.ms;
      var new_thread = new TLThread(e.thread, e.ms, end_ms);
      this.threads_by_id[new_thread.id] = this.threads.length;
      this.threads.push(new_thread);
    }

    var thread = this.threads[this.threads_by_id[e.thread]];
    thread.AddEvent(e);

    // Keep trace of the time of the last event seen.
    var end_ms = e.done ? e.done : e.ms;
    if (end_ms > this.endms) this.endms = end_ms;
    if (end_ms > thread.endms) thread.endms = end_ms;

    switch(e.eventtype) {
      case 'EVENT_TYPE_THREADNAME':
        thread.name = e.e['threadname'];
        break;
      case 'EVENT_TYPE_CREATETHREAD':
        tiez[e.e['eventid']] = e;
        break;
      case 'EVENT_TYPE_THREADBEGIN':
        var pei = e.e['parenteventid'];
        if (pei in tiez) {
          e.parentevent = tiez[pei];
          tiez[pei].childevent = e;
        }
        break;
    }
  }
};

Traceline.prototype.Render =
function() { this.RenderSVG(); };

Traceline.prototype.RenderText =
function() {
  var z = document.getElementsByTagNameNS(xhtmlNS, 'body')[0];
  for (var i = 0, il = this.threads.length; i < il; ++i) {
    var p = document.createElementNS(
      'http://www.w3.org/1999/xhtml', 'p');
    p.innerHTML = this.threads[i].toString();
    z.appendChild(p);
  }
};

// So here we go.  For two reasons, I implement my own scrolling system.
// First off, is that in order to scale, we want to have as little on the DOM
// as possible.  This means not having off-screen elements in the DOM, as this
// slows down everything.  This comes at a cost of more expensive
// scrolling performance since you have to re-render the scene.  The second
// reason is a bug I stumbled into:
//  https://bugs.webkit.org/show_bug.cgi?id=21968
// This means that scrolling an SVG element doesn't really work properly
// anyway.  So what the code does is this.  We have our layout that looks like:
// [ thread names ] [ svg timeline ]
//                  [ scroll bar ]
// We make a fake scrollbar, which doesn't actually have the SVG inside of it,
// we want for when this scrolls, with some debouncing, and then when it has
// scrolled we rerender the scene.  This means that the SVG element is never
// scrolled, and coordinates are always at 0.  We keep the scene in millisecond
// units which also helps for zooming.  We do our own hit testing and decide
// what needs to be renderer, convert from milliseconds to SVG pixels, and then
// draw the update into the static SVG element...  Y coordinates are still
// always in pixels (since we aren't paging along the Y axis), but this might
// be something to fix up later.

function SVGSceneLine(msg, klass, x1, y1, x2, y2) {
  this.type = SVGSceneLine;
  this.msg = msg;
  this.klass = klass;

  this.x1 = x1;
  this.y1 = y1;
  this.x2 = x2;
  this.y2 = y2;

  this.hittest = function(startms, dur) {
    return true;
  };
}

function SVGSceneRect(msg, klass, x, y, width, height) {
  this.type = SVGSceneRect;
  this.msg = msg;
  this.klass = klass;

  this.x = x;
  this.y = y;
  this.width = width;
  this.height = height;

  this.hittest = function(startms, dur) {
    return this.x <= (startms + dur) &&
           (this.x + this.width) >= startms;
  };
}

Traceline.prototype.RenderSVG =
function() {
  var threadnames = this.RenderSVGCreateThreadNames();
  var scene = this.RenderSVGCreateScene();

  var curzoom = 8;

  // The height is static after we've created the scene
  var dom = this.RenderSVGCreateDOM(threadnames, scene.height);

  dom.zoom(curzoom);

  dom.attach();

  var draw = (function(obj) {
    return function(scroll, total) {
      var startms = (scroll / total) * obj.endms;

      var start = (new Date).getTime();
      var count = obj.RenderSVGRenderScene(dom, scene, startms, curzoom);
      var total = (new Date).getTime() - start;

      dom.infoareadiv.innerHTML =
          'Scene render of ' + count + ' nodes took: ' + total + ' ms';
    };
  })(this, dom, scene);

  // Paint the initial paint with no scroll
  draw(0, 1);

  // Hook us up to repaint on scrolls.
  dom.redraw = draw;
};


// Create all of the DOM elements for the SVG scene.
Traceline.prototype.RenderSVGCreateDOM =
function(threadnames, svgheight) {

  // Total div holds the container and the info area.
  var totaldiv = document.createElementNS(xhtmlNS, 'div');

  // Container holds the thread names, SVG element, and fake scroll bar.
  var container = document.createElementNS(xhtmlNS, 'div');
  container.className = 'container';

  // This is the div that holds the thread names along the left side, this is
  // done in HTML for easier/better text support than SVG.
  var threadnamesdiv = document.createElementNS(xhtmlNS, 'div');
  threadnamesdiv.className = 'threadnamesdiv';

  // Add all of the names into the div, these are static and don't update.
  for (var i = 0, il = threadnames.length; i < il; ++i) {
    var div = document.createElementNS(xhtmlNS, 'div');
    div.className = 'threadnamediv';
    div.appendChild(document.createTextNode(threadnames[i]));
    threadnamesdiv.appendChild(div);
  }

  // SVG div goes along the right side, it holds the SVG element and our fake
  // scroll bar.
  var svgdiv = document.createElementNS(xhtmlNS, 'div');
  svgdiv.className = 'svgdiv';

  // The SVG element, static width, and we will update the height after we've
  // walked through how many threads we have and know the size.
  var svg = document.createElementNS(svgNS, 'svg');
  svg.setAttributeNS(null, 'height', svgheight);
  svg.setAttributeNS(null, 'width', this.kTimelineWidthPx);

  // The fake scroll div is an outer div with a fixed size with a scroll.
  var fakescrolldiv = document.createElementNS(xhtmlNS, 'div');
  fakescrolldiv.className = 'fakescrolldiv';

  // Fatty is inside the fake scroll div to give us the size we want to scroll.
  var fattydiv = document.createElementNS(xhtmlNS, 'div');
  fattydiv.className = 'fattydiv';
  fakescrolldiv.appendChild(fattydiv);

  var infoareadiv = document.createElementNS(xhtmlNS, 'div');
  infoareadiv.className = 'infoareadiv';
  infoareadiv.innerHTML = 'Hover an event...';

  // Set the SVG mouseover handler to write the data to the infoarea.
  svg.addEventListener('mouseover', (function(infoarea) {
    return function(e) {
      if ('msg' in e.target && e.target.msg) {
        infoarea.innerHTML = e.target.msg;
      }
      e.stopPropagation();  // not really needed, but might as well.
    };
  })(infoareadiv), true);


  svgdiv.appendChild(svg);
  svgdiv.appendChild(fakescrolldiv);

  container.appendChild(threadnamesdiv);
  container.appendChild(svgdiv);

  totaldiv.appendChild(container);
  totaldiv.appendChild(infoareadiv);

  var widthms = Math.floor(this.endms + 2);
  // Make member variables out of the things we want to 'export', things that
  // will need to be updated each time we redraw the scene.
  var obj = {
    // The root of our piece of the DOM.
    'totaldiv': totaldiv,
    // We will want to listen for scrolling on the fakescrolldiv
    'fakescrolldiv': fakescrolldiv,
    // The SVG element will of course need updating.
    'svg': svg,
    // The area we update with the info on mouseovers.
    'infoareadiv': infoareadiv,
    // Called when we detected new scroll a should redraw
    'redraw': function() { },
    'attached': false,
    'attach': function() {
      document.getElementsByTagNameNS(xhtmlNS, 'body')[0].appendChild(
          this.totaldiv);
      this.attached = true;
    },
    // The fatty div will have its width adjusted based on the zoom level and
    // the duration of the graph, to get the scrolling correct for the size.
    'zoom': function(curzoom) {
      var width = widthms * curzoom;
      fattydiv.style.width = width + 'px';
    },
    'detach': function() {
      this.totaldiv.parentNode.removeChild(this.totaldiv);
      this.attached = false;
    },
  };

  // Watch when we get scroll events on the fake scrollbar and debounce.  We
  // need to give it a pointer to use in the closer to call this.redraw();
  fakescrolldiv.addEventListener('scroll', (function(theobj) {
    var seqnum = 0;
    return function(e) {
      seqnum = (seqnum + 1) & 0xffff;
      window.setTimeout((function(myseqnum) {
        return function() {
          if (seqnum == myseqnum) {
            theobj.redraw(e.target.scrollLeft, e.target.scrollWidth);
          }
        };
      })(seqnum), 100);
    };
  })(obj), false);

  return obj;
};

Traceline.prototype.RenderSVGCreateThreadNames =
function() {
  // This names is the list to show along the left hand size.
  var threadnames = [ ];

  for (var i = 0, il = this.threads.length; i < il; ++i) {
    var thread = this.threads[i];

    // TODO make this not so stupid...
    if (i != 0) {
      for (var j = 0; j < this.thread_set_indexes.length; j++) {
        if (i == this.thread_set_indexes[j]) {
          threadnames.push('------');
          break;
        }
      }
    }

    threadnames.push(thread.name);
  }

  return threadnames;
};

Traceline.prototype.RenderSVGCreateScene =
function() {
  // This scene is just a list of SVGSceneRect and SVGSceneLine, in no great
  // order.  In the future they should be structured to make range checking
  // faster.
  var scene = [ ];

  // Remember, for now, Y (height) coordinates are still in pixels, since we
  // don't zoom or scroll in this direction.  X coordinates are milliseconds.

  var lasty = 0;
  for (var i = 0, il = this.threads.length; i < il; ++i) {
    var thread = this.threads[i];

    // TODO make this not so stupid...
    if (i != 0) {
      for (var j = 0; j < this.thread_set_indexes.length; j++) {
        if (i == this.thread_set_indexes[j]) {
          lasty += this.kThreadHeightPx;
          break;
        }
      }
    }

    // For this thread, create the background thread (blue band);
    scene.push(new SVGSceneRect(null,
                                'thread',
                                thread.startms,
                                1 + lasty,
                                thread.duration_ms(),
                                this.kThreadHeightPx - 2));

    // Now create all of the events...
    var pushdown = [ 0, 0, 0, 0 ];
    for (var j = 0, jl = thread.events.length; j < jl; ++j) {
      var e = thread.events[j];

      var y = 2 + lasty;

      // TODO this is a hack just so that we know the correct why position
      // so we can create the threadline...
      if (e.childevent) {
        e.marky = y;
      }

      // Handle events that we want to represent as lines and not event blocks,
      // right now this is only thread creation.  We map an event back to its
      // "parent" event, and now lets add a line to represent that.
      if (e.parentevent) {
        var eparent = e.parentevent;
        var msg = eparent.toString() + '<br/>' + e.toString();
        scene.push(
            new SVGSceneLine(msg, 'eventline',
                             eparent.ms, eparent.marky + 5, e.ms, lasty + 5));
      }

      // We get negative done values (well, really, it was 0 and then made
      // relative to start time) when a syscall never returned...
      var dur = 0;
      if ('done' in e.e && e.e['done'] > 0) {
        dur = e.e['done'] - e.ms;
      }

      // TODO skip short events for now, but eventually we should figure out
      // a way to control this from the UI, etc.
      if (dur < 0.2)
        continue;

      var width = dur;

      // Try to find an available horizontal slot for our event.
      for (var z = 0; z < pushdown.length; ++z) {
        var found = false;
        var slot = z;
        if (pushdown[z] < e.ms) {
          found = true;
        }
        if (!found) {
          if (z != pushdown.length - 1)
            continue;
          slot = Math.floor(Math.random() * pushdown.length);
          alert('blah');
        }

        pushdown[slot] = e.ms + dur;
        y += slot * 4;
        break;
      }


      // Create the event
      klass = e.e.waiting ? 'eventwaiting' : 'event';
      scene.push(
          new SVGSceneRect(e.toString(), klass, e.ms, y, width, 3));

      // If there is a "parentevent", we want to make a line there.
      // TODO
    }

    lasty += this.kThreadHeightPx;
  }

  return {
    'scene': scene,
    'width': this.endms + 2,
    'height': lasty,
  };
};

Traceline.prototype.RenderSVGRenderScene =
function(dom, scene, startms, curzoom) {
  var stuff = scene.scene;
  var svg = dom.svg;

  var count = 0;

  // Remove everything from the DOM.
  while (svg.firstChild)
    svg.removeChild(svg.firstChild);

  // Don't actually need this, but you can't transform on an svg element,
  // so it's nice to have a <g> around for transforms...
  var svgg = document.createElementNS(svgNS, 'g');

  var dur = this.kTimelineWidthPx / curzoom;

  function min(a, b) {
    return a < b ? a : b;
  }

  function max(a, b) {
    return a > b ? a : b;
  }

  function timeToPixel(x) {
    // TODO(deanm): This clip is a bit shady.
    var x = min(max(Math.floor(x*curzoom), -100), 2000);
    return (x == 0 ? 1 : x);
  }

  for (var i = 0, il = stuff.length; i < il; ++i) {
    var thing = stuff[i];
    if (!thing.hittest(startms, startms+dur))
      continue;


    if (thing.type == SVGSceneRect) {
      var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      rect.setAttributeNS(null, 'class', thing.klass)
      rect.setAttributeNS(null, 'x', timeToPixel(thing.x - startms));
      rect.setAttributeNS(null, 'y', thing.y);
      rect.setAttributeNS(null, 'width', timeToPixel(thing.width));
      rect.setAttributeNS(null, 'height', thing.height);
      rect.msg = thing.msg;
      svgg.appendChild(rect);
    } else if (thing.type == SVGSceneLine) {
      var line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttributeNS(null, 'class', thing.klass)
      line.setAttributeNS(null, 'x1', timeToPixel(thing.x1 - startms));
      line.setAttributeNS(null, 'y1', thing.y1);
      line.setAttributeNS(null, 'x2', timeToPixel(thing.x2 - startms));
      line.setAttributeNS(null, 'y2', thing.y2);
      line.msg = thing.msg;
      svgg.appendChild(line);
    }

    ++count;
  }

  // Append the 'g' element on after we've build it.
  svg.appendChild(svgg);

  return count;
};
