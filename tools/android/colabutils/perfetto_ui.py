# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
from IPython.display import display, Javascript


def open_trace(trace):
    """Executes JavaScript to open a local trace in the Perfetto UI directly in
    the browser.

    Args:
      trace: The trace file object
    """
    trace_file_path = trace.trace_file

    # Read the trace file content and encode in base64 to embed it in the
    # JavaScript
    with open(trace.trace_file, 'rb') as f:
        trace_base64 = base64.b64encode(f.read()).decode('utf-8')

    # Generate the JavaScript code to open the Perfetto UI and post the trace
    # data. See:
    # https://perfetto.dev/docs/visualization/deep-linking-to-perfetto-ui
    js_code = f"""
        const trace_base64 = "{trace_base64}";
        const trace_title = "{trace_file_path}";

        function base64ToArrayBuffer(base64) {{
            const binary_string = window.atob(base64);
            const len = binary_string.length;
            const bytes = new Uint8Array(len);
            for (let i = 0; i < len; i++) {{
                bytes[i] = binary_string.charCodeAt(i);
            }}
            return bytes.buffer;
        }}

        function openPerfettoUI() {{
            const perfettoUI = window.open('https://ui.perfetto.dev');

            if (!perfettoUI) {{
                console.error('Error: Pop-up was blocked. Please allow pop-ups for this page.');
                return;
            }}

            const traceBuffer = base64ToArrayBuffer(trace_base64);

            const pingInterval = setInterval(() => {{
              try {{
                perfettoUI.postMessage('PING', 'https://ui.perfetto.dev');
              }} catch (e) {{
                console.error("Error sending PING:", e);
                clearInterval(pingInterval);
              }}
            }}, 50);

            const messageHandler = (event) => {{
                if (event.origin !== 'https://ui.perfetto.dev') {{
                    return;
                }}
                if (event.data === 'PONG') {{
                    clearInterval(pingInterval);
                    window.removeEventListener('message', messageHandler); // Remove listener after receiving PONG
                    try {{
                      perfettoUI.postMessage({{
                          perfetto: {{
                              buffer: traceBuffer,
                              title: trace_title,
                              fileName: trace_title
                          }}
                      }}, 'https://ui.perfetto.dev');
                    }} catch (e) {{
                       console.error("Error posting trace data:", e);
                    }}
                }}
            }};

            window.addEventListener('message', messageHandler);
        }}

        // Immediately call the function to open the Perfetto UI
        openPerfettoUI();
    """

    # Execute the JavaScript directly in the browser
    display(Javascript(js_code))
