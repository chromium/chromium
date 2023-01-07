// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

google.load("visualization", "1", {packages:["treemap"]});
google.setOnLoadCallback(drawChart);
function drawChart() {
  var data = google.visualization.arrayToDataTable(g_raw_data);

  tree = new google.visualization.TreeMap(
      document.getElementById('chart_div'));

  tree.draw(data, {
    minColor: '#faa',
    midColor: '#f77',
    maxColor: '#f44',
    headerHeight: 20,
    fontColor: 'black',
    showScale: true,
    minColorValue: 0,
    maxColorValue: g_maxval,
    generateTooltip: tooltip
  });

  // Update from 'Loading'.
  document.getElementById('title').innerText = g_dllname;

  // Set favicon.
  var doc_head = document.getElementsByTagName('head')[0];
  var new_link = document.createElement('link');
  new_link.rel = 'shortcut icon';
  new_link.href = 'data:image/png;base64,'+g_favicon;
  doc_head.appendChild(new_link);

  var cur_line_sizes = null;
  function nodeSelect() {
    symlist.setValue('');
    var selected = tree.getSelection();
    if (selected.length > 0) {
      var filename = data.getValue(selected[0].row, 0);
      var size = data.getValue(selected[0].row, 2);
      if (size >= 0) {
        // Is a leaf.
        cur_line_sizes = g_line_data[filename];
        var body = g_file_contents[filename];
        editor.setValue(body);
        var maximum_size = 0;
        for (var line in cur_line_sizes) {
          maximum_size = Math.max(maximum_size, cur_line_sizes[line][0]);
        }
        for (var line in cur_line_sizes) {
          var symbol_indices = cur_line_sizes[line][1];
          var symbols = [];
          for (var i = 0; i < symbol_indices.length; ++i) {
            symbols.push(g_symbol_list[symbol_indices[i]]);
          }
          var size = cur_line_sizes[line][0];
          // Zero based lines.
          var line_num = parseInt(line, 10) - 1;
          if (size >= maximum_size * 0.9)
            editor.addLineClass(line_num, 'gutter', 'linebg-top10');
          else if (size >= maximum_size * 0.75)
            editor.addLineClass(line_num, 'gutter', 'linebg-top25');
          else if (size >= maximum_size * 0.5)
            editor.addLineClass(line_num, 'gutter', 'linebg-top50');
          function addTag() {
            var line_num = parseInt(line, 10);
            var symbols_tooltip = symbols.join('\n');
            var num_syms = symbols.length;
            // markText wants 0-based lines.
            var mark = editor.markText({line: line_num - 1, ch: 0},
                            {line: line_num, ch: 0},
                            { className: 'symbol-tag' });
            CodeMirror.on(mark, 'beforeCursorEnter', function(e) {
              symlist.setValue(num_syms +
                                ' symbol(s) contributing to line ' +
                                line_num + ':\n' +
                                symbols_tooltip);
            });
          }
          addTag();
        }
      }
    }
  }
  google.visualization.events.addListener(tree, 'select', nodeSelect);

  editor = CodeMirror.fromTextArea(
    document.getElementById('source_view'), {
      readOnly: "nocursor",
      mode: { name: 'text/x-c++src' },
      lineNumbers: true,
      lineNumberFormatter: weightGetter
    });
  editor.setSize(850, 600);

  symlist = CodeMirror.fromTextArea(
    document.getElementById('symlist_view'), {
      readOnly: "nocursor",
      mode: { name: 'none' },
      lineNumbers: false
    });
  symlist.setSize(850, 150);

  function tooltip(row, size, value) {
    return '<div style="background:#fd9;' +
            ' padding:10px; border-style:solid"><b>' +
            data.getValue(row, 0) + '</b><br>' +
            data.getColumnLabel(2) +
            ' (total value of this cell and its children): ' + size +
            '<br>';
  }

  function weightGetter(line) {
    if (cur_line_sizes && cur_line_sizes.hasOwnProperty('' + line)) {
      return cur_line_sizes['' + line][0] + ' bytes ' + line;
    }
    return line;
  }
}
