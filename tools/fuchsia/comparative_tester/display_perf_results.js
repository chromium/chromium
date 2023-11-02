// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const headers = ["Test", "Line", "Units", "Fuchsia Avg", "Fuchsia Dev",
                       "Fuchsia CV", "Linux Avg", "Linux Dev", "Linux CV",
                       "F-L Avgs"];

function generateHeader() {
  // Generates the header for a table of statistics.
  let header_row = document.createElement("tr");
  for (let i = 0; i < headers.length; i++){
    let header = document.createElement("th");
    header.appendChild(document.createTextNode(headers[i]));
    header_row.appendChild(header);
  }
  return header_row;
}

function generateTableHtmlFromOutputRows(output_rows) {
  // Takes a list of rows of data, and collects them together into a single
  // table.
  let table = document.createElement("table");
  table.appendChild(generateHeader());
  output_rows.forEach(function(row){
    let doc_row = document.createElement("tr");
    row.forEach(function(datum){
      let item = document.createElement("td");
      item.appendChild(document.createTextNode(datum));
      doc_row.appendChild(item);
    });
    table.appendChild(doc_row);
  });
  return table;
}

function extractStats(line) {
  // Deconstructs the JSON file given into a list of the relevant values.
  return [line.fuchsia_avg,
          line.fuchsia_dev,
          line.fuchsia_cv,
          line.linux_avg,
          line.linux_dev,
          line.linux_cv,
          line.fuchsia_avg - line.linux_avg];
}

function renderTable(obj_dict) {
  // Returns an HTML table object by taking the TargetResults JSON and using it
  // to populate a table of statistics.
  let rows = []

  const name = obj_dict['name']
  const tests = obj_dict['tests'];
  Object.keys(tests).forEach(function(test_name) {
    const test = tests[test_name];
    const test_stats = extractStats(test)
    let test_row = [test_name, "Test Totals", test['unit']].concat(test_stats);
    rows.push(test_row)
    const lines = test['lines']
    Object.keys(lines).forEach(function(line_key) {
      const line = lines[line_key];
      const line_stats = extractStats(line);
      let line_row = [test_name, line_key, line['unit']].concat(line_stats);
      rows.push(line_row);
    });
    rows.push([]);
  });
  return generateTableHtmlFromOutputRows(rows);
}

function loadTable() {
  // Reads the result files, and adds the tables generated to the table_div HTML
  // object as they finish loading.
  let files = document.getElementById('files').files;
  let table_div = document.getElementById('stats_div');
  // Clear the table div of all prior stats tables
  while (table_div.firstChild)
    table_div.removeChild(table_div.firstChild);

  for (let i = 0; i < files.length; i++){
    let file = files[0];
    let reader = new FileReader();

    reader.addEventListener('loadend', function(contents) {
      let json_parsed = JSON.parse(reader.result)
      let table = renderTable(json_parsed);
      table_div.appendChild(table);
    });
    reader.readAsBinaryString(file);
  }
}
