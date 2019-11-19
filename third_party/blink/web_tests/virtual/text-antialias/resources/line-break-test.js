let container = document.getElementById("container");
let breakTestFontsize = parseFloat(getComputedStyle(container).fontSize);
let nbsp = String.fromCharCode(0xA0);
nbsp = nbsp + nbsp;

class LineBreakTestElement {
  constructor(last, current) {
    let element = document.createElement('div');
    element.textContent = nbsp + String.fromCharCode(last) + String.fromCharCode(current);
    container.appendChild(element);
    this.element = element;
  }

  get canBreak() {
    return this.element.offsetHeight / breakTestFontsize > 1.9;
  }
}

class LineBreakTest {
  constructor(begin, end) {
    let rows = [];
    for (let last = begin; last < end; last++) {
      let row = [];
      for (let current = begin; current < end; current++)
        row.push(new LineBreakTestElement(last, current));
      rows.push(row);
    }
    this.begin = begin;
    this.rows = rows;
  }

  toResultString() {
    let header = [];
    for (let i = 0; i < this.rows.length; i++)
      header.push(String.fromCharCode(this.begin + i));
    let rows = ["  " + header.join("")];
    for (let i = 0; i < this.rows.length; i++) {
      let row = String.fromCharCode(this.begin + i) + " " +
          this.rows[i].map((test) => test.canBreak ? "/" : "X")
          .join("");
      rows.push(row);
    }
    return rows.join("\n");
  }
}
