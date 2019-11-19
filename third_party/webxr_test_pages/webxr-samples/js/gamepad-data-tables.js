// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

class GamepadTable {
  constructor(title, cols, parent) {
    this.table = document.createElement("table");
    this.table.setAttribute("border", 1);
    this.body = document.createElement("tbody");
    this.AddHeader(title, cols);
    this.table.appendChild(this.body);
    parent.appendChild(this.table);
  }

  AddHeader(title, cols) {
    let row = document.createElement("tr");
    let th = document.createElement("th");
    th.setAttribute("colspan", cols);
    th.appendChild(document.createTextNode(title));
    row.appendChild(th);
    this.body.appendChild(row);
  }

  AddCell(row, text) {
    let cell = document.createElement("td");
    cell.appendChild(document.createTextNode(text));
    row.appendChild(cell);
    return cell;
  }

  AddRow(values) {
    let cells = [];
    let row = document.createElement("tr");
    for (let i = 0; i < values.length; ++i) {
      cells.push(this.AddCell(row, values[i]));
    }
    this.body.appendChild(row);
    return cells;
  }
}

class ButtonTable extends GamepadTable {
  constructor(buttons, parent) {
    super("button data", 3, parent);

    this.AddRow(["pressed", "touched", "value"]);

    this.pressed_cells = [];
    this.touched_cells = [];
    this.value_cells = [];

    this.pressed = [];
    this.touched = [];
    this.values = [];

    for (let i = 0; i < buttons.length; ++i) {
      this.pressed.push(buttons[i].pressed);
      this.touched.push(buttons[i].touched);
      this.values.push(buttons[i].value);
      let cells = this.AddRow([buttons[i].pressed, buttons[i].touched, buttons[i].value.toFixed(3)]);
      this.pressed_cells.push(cells[0]);
      this.touched_cells.push(cells[1]);
      this.value_cells.push(cells[2]);
    }
  }

  update(buttons) {
    for (let i = 0; i < buttons.length; ++i) {
      const is_pressed = buttons[i].pressed;
      if (this.pressed[i] != is_pressed) {
        this.pressed_cells[i].innerHTML = is_pressed;
        this.pressed[i] = is_pressed;
      }
      const is_touched = buttons[i].touched;
      if (this.touched[i] != is_touched) {
        this.touched_cells[i].innerHTML = is_touched;
        this.touched[i] = is_touched;
      }
      const value = buttons[i].value;
      if (this.values[i] != value) {
        this.value_cells[i].innerHTML = value.toFixed(3);
        this.values[i] = value;
      }
    }
  }
}

class AxesTable extends GamepadTable {
  constructor(axes, parent) {
    super("axis values", 1, parent);

    this.values = [];
    for (let i = 0; i < axes.length; ++i) {
      this.values.push(axes[i]);
    }

    this.cells = [];
    for (let i = 0; i < axes.length; ++i) {
      let temp_cells = this.AddRow([axes[i].toFixed(3)]);
      this.cells.push(temp_cells[0]);
    }
  }

  update(axes) {
    // assumes length is still the same
    for (let i = 0; i < axes.length; ++i) {
      if (this.values[i] != axes[i]) {
        this.cells[i].innerHTML = axes[i].toFixed(3);
        this.values[i] = axes[i];
      }
    }
  }
}

class InfoTable extends GamepadTable {
  constructor(gamepad, profiles, parent) {
    super("Gamepad", 2, parent);

    this.id = gamepad.id;
    this.mapping = gamepad.mapping;
    this.index = gamepad.index;
    this.profiles_string = profiles.toString();

    this.id_cell = this.AddRow(["id", gamepad.id])[1];
    this.mapping_cell = this.AddRow(["mapping", gamepad.mapping])[1];
    this.index_cell = this.AddRow(["index", gamepad.index])[1];
    this.profiles_cell = this.AddRow(["profiles", this.profiles_string])[1];

    this.timestamp_cell = this.AddRow(["timestamp", gamepad.timestamp])[1];
  }

  update(gamepad, profiles) {
    if (this.id != gamepad.id) {
      this.id_cell.innerHTML = gamepad.id;
      this.id = gamepad.id;
    }
    if (this.mapping != gamepad.mapping) {
      this.mapping_cell.innerHTML = gamepad.mapping;
      this.mapping = gamepad.mapping;
    }
    if (this.index != gamepad.index) {
      this.index_cell.innerHTML = gamepad.index;
      this.index = gamepad.index;
    }
    let profiles_string = profiles.toString();
    if (this.profiles_string != profiles_string) {
      this.profiles_cell.innerHTML = profiles_string;
      this.profiles_string = profiles_string;
    }

    // Most recent time that this gamepad's input state (buttons + axes) has
    // changed. Measured as milliseconds relative to the page's navigation
    // start.
    this.timestamp_cell.innerHTML = gamepad.timestamp.toFixed(3);
  }
}

export class GamepadTableManager {
  constructor() {
    this.frame_number = 0;
    this.tables = {};
  }

  nextFrame() {
    this.frame_number++;
  }

  update(input_source) {
    // Construct the tables if necessary. Must check this every frame
    // because otherwise, the table doesn't get created until the gamepad
    // has an input change on a frame that's a multiple of 10.
    let hand = input_source.handedness;
    let gamepad = input_source.gamepad;
    let profiles = input_source.profiles;
    if (!(hand in this.tables)) {
      let div = document.getElementById("gamepad-details");
      let header = document.createElement("header");
      let details = document.createElement("details");
      details.setAttribute("id", "gamepad-details-hand-" + hand);
      details.setAttribute("open", "");
      let summary = document.createElement("summary");
      summary.innerHTML = hand + "-hand Gamepad";
      let p = document.createElement("p");
      p.innerHTML = "Real-time info for gamepad associated with " + hand + " hand.";
      details.appendChild(summary);
      details.appendChild(p);
      header.appendChild(details);
      div.appendChild(header);

      this.tables[hand] = {
        info : new InfoTable(gamepad, profiles, details),
        axes : new AxesTable(gamepad.axes, details),
        buttons : new ButtonTable(gamepad.buttons, details)
      };
    }

    // Only update the gamepad tables once every 10 frames for perf reasons.
    if ((this.frame_number % 10) == 0) {
      this.tables[hand].info.update(gamepad, profiles);
      this.tables[hand].axes.update(gamepad.axes);
      this.tables[hand].buttons.update(gamepad.buttons);
    }
  }
}
