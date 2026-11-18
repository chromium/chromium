# Copyright 2026 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""E2E tests for running several BiDi Mapper instances in one Mapper tab.

Setup used by every test:

* The primary Mapper is started by ChromeDriver in the "BiDi-CDP Mapper" tab
  with `window.runMapperInstance(selfTargetId)`. It uses the default transports:
  `WindowBidiTransport` (`window.onBidiMessage` / `window.sendBidiResponse`)
  and `WindowCdpTransport` (`window.cdp`). The `websocket` fixture talks to it.
* The tests start more Mapper instances in the same tab with
  `window.runMapperInstance(selfTargetId, bidiTransport, cdpTransport?)` and
  talk to them through `InTabMapperConnection`:

      test (Python)
        |
        |  control CDP WebSocket (`_CdpConnection`), attached to the Mapper tab
        |    BiDi in:  Runtime.evaluate("onBidiMessage_<id>(message)")
        |    BiDi out: Runtime.bindingCalled("sendBidiResponse_<id>")
        v
      Mapper tab: BidiServer instance <id>
        |
        |  CDP, one of:
        |    a) custom transport (default in tests): messages are relayed by
        |       the test over a separate browser WebSocket
        |       (Runtime.bindingCalled("sendCdpMessage_<id>") out,
        |        Runtime.evaluate("onCdpMessage_<id>(message)") in);
        |    b) no custom transport: shares `window.cdp` with the primary Mapper
        |       (one `MapperCdpConnection`, one browser session per Mapper).
        v
      browser

`InTabMapperConnection` implements `send()` and `recv()` like a WebSocket, so
the usual `test_helpers` (`execute_command`, `subscribe`, `wait_for_event`, ...)
work with it in the same way as with the `websocket` fixture.
"""

import asyncio
import itertools
import json
import os
from collections.abc import Callable

import pytest
import pytest_asyncio
import websockets
from anys import ANY_STR
from test_helpers import (
    ANY_TIMESTAMP,
    AnyExtending,
    execute_command,
    get_mapper_target_id,
    get_tree,
    goto_url,
    read_JSON_message,
    send_JSON_command,
    subscribe,
    wait_for_event,
    wait_for_filtered_event,
)


async def wait_for_command_and_event(
    websocket, command_id: int, event_method: str, timeout: int = 20
) -> tuple[dict, dict]:
    """Waits for both a command result and an event on the same connection,
    regardless of the order in which they arrive.

    The event is matched by method prefix. Other messages read in the meantime
    are dropped.
    """
    command_result = None
    event_response = None

    async def _wait():
        nonlocal command_result, event_response
        while command_result is None or event_response is None:
            resp = await read_JSON_message(websocket)
            if resp.get("id") == command_id:
                if "result" in resp:
                    command_result = resp["result"]
                else:
                    raise Exception(
                        {"error": resp["error"], "message": resp["message"]}
                    )
            elif resp.get("type") == "event" and resp.get("method", "").startswith(
                event_method
            ):
                event_response = resp

    await asyncio.wait_for(_wait(), timeout)
    assert command_result is not None
    assert event_response is not None
    return command_result, event_response


class _CdpConnection:
    """Minimal CDP client over a direct WebSocket to the browser.

    A background task reads all incoming messages: command responses resolve
    the future of the matching `send_command` call, and events are passed to
    the handlers registered with `on_event`.
    """

    def __init__(self, ws: websockets.WebSocketClientProtocol):
        self._ws = ws
        self._id_counter = itertools.count(1)
        # Command ID -> future resolved with the raw CDP response.
        self._pending: dict[int, asyncio.Future[dict]] = {}
        self._event_handlers: list[Callable[[dict], None]] = []
        self._reader_task = asyncio.create_task(self._reader_loop())

    async def _reader_loop(self) -> None:
        error: Exception | None = None
        try:
            async for raw_message in self._ws:
                message = json.loads(raw_message)
                if "id" in message:
                    # Command response.
                    future = self._pending.pop(message["id"], None)
                    if future is not None and not future.done():
                        future.set_result(message)
                else:
                    # Event.
                    for handler in list(self._event_handlers):
                        handler(message)
        except Exception as e:
            error = e
        finally:
            # The socket is closed or broken: fail all pending commands, so no
            # `send_command` call waits forever.
            close_exc = error or ConnectionError("CDP WebSocket connection closed")
            for future in self._pending.values():
                if not future.done():
                    future.set_exception(close_exc)
            self._pending.clear()

    def on_event(self, handler: Callable[[dict], None]) -> None:
        self._event_handlers.append(handler)

    def off_event(self, handler: Callable[[dict], None]) -> None:
        if handler in self._event_handlers:
            self._event_handlers.remove(handler)

    async def send_command(
        self, method: str, params: dict | None = None, session_id: str | None = None
    ) -> dict:
        """Sends a CDP command and returns its result. Raises on CDP error."""
        cmd_id = next(self._id_counter)
        payload: dict = {"id": cmd_id, "method": method, "params": params or {}}
        if session_id is not None:
            payload["sessionId"] = session_id

        loop = asyncio.get_running_loop()
        future: asyncio.Future[dict] = loop.create_future()
        # Register the future before sending, so the response cannot be missed.
        self._pending[cmd_id] = future

        await self._ws.send(json.dumps(payload))
        resp = await future
        if "error" in resp:
            raise RuntimeError(f"CDP error in {method}: {resp['error']}")
        return resp["result"]

    async def close(self) -> None:
        self._reader_task.cancel()
        await asyncio.gather(self._reader_task, return_exceptions=True)
        await self._ws.close()


class InTabMapperConnection:
    """A websocket-compatible connection to a BidiServer instance running inside
    the BiDi-CDP Mapper tab with its own isolated BiDi (and optional CDP)
    transport.

    Message flow (see the module docstring for the overview):

    * BiDi, test -> Mapper: `send()` evaluates `onBidiMessage_<id>(message)` in
      the Mapper tab over the shared control CDP connection.
    * BiDi, Mapper -> test: the Mapper calls the `sendBidiResponse_<id>`
      binding. The control connection receives `Runtime.bindingCalled`, and
      `_on_control_cdp_event` puts the message into `_bidi_queue`, which is
      read by `recv()`.
    * CDP (only with `mapper_cdp_ws`): the Mapper calls the
      `sendCdpMessage_<id>` binding, and `_forward_cdp_to_browser` sends the
      message over `mapper_cdp_ws`. Messages from the browser are delivered
      back by `_forward_cdp_from_browser`, which evaluates
      `onCdpMessage_<id>(message)` in the Mapper tab.

    Without `mapper_cdp_ws`, the Mapper uses the default `window.cdp`
    transport, shared with the primary Mapper.
    """

    def __init__(
        self,
        control_cdp: _CdpConnection,
        mapper_cdp_ws: websockets.WebSocketClientProtocol | None,
        mapper_session_id: str,
        instance_id: str,
    ):
        # CDP connection attached to the Mapper tab. Shared by all instances.
        self._control_cdp = control_cdp
        # Browser WebSocket for the custom CDP transport, or `None` to use the
        # default `window.cdp` transport.
        self._mapper_cdp_ws = mapper_cdp_ws
        # CDP session of `_control_cdp` attached to the Mapper tab.
        self._mapper_session_id = mapper_session_id
        self._instance_id = instance_id
        # The in-tab Mapper was closed (`closeMapper_<id>` was called).
        self._mapper_closed = False
        # The Python side was released (tasks cancelled, WebSocket closed).
        self._closed = False
        # All BiDi events received from the Mapper, including events dropped by
        # helpers like `wait_for_event`. Used to check that an event was NOT
        # received.
        self.received_events: list[dict] = []
        # BiDi messages from the Mapper, read by `recv()`. Errors of the
        # background tasks are put here too, so a waiting test fails instead of
        # timing out.
        self._bidi_queue: asyncio.Queue[str | BaseException] = asyncio.Queue()
        # CDP messages from the Mapper waiting to be sent to the browser.
        self._cdp_send_queue: asyncio.Queue[str] = asyncio.Queue()
        # Set when the browser reports that the Mapper's browser session was
        # detached (see `_forward_cdp_from_browser`).
        self._browser_session_detached = asyncio.Event()
        self._cdp_send_task: asyncio.Task[None] | None = None
        self._cdp_forward_task: asyncio.Task[None] | None = None
        if self._mapper_cdp_ws is not None:
            self._cdp_send_task = asyncio.create_task(self._forward_cdp_to_browser())
            self._cdp_forward_task = asyncio.create_task(
                self._forward_cdp_from_browser()
            )

        # Global names in the Mapper tab. They are unique per instance, so
        # instances do not overwrite each other's bindings and handlers.
        # Bindings called by the Mapper (in-tab -> test):
        self._send_bidi_binding = f"sendBidiResponse_{instance_id}"
        self._send_cdp_binding = f"sendCdpMessage_{instance_id}"
        # Functions called by the test (test -> in-tab):
        self._on_bidi_fn = f"onBidiMessage_{instance_id}"
        self._on_cdp_fn = f"onCdpMessage_{instance_id}"
        self._close_fn = f"closeMapper_{instance_id}"

        self._control_cdp.on_event(self._on_control_cdp_event)

    def _on_control_cdp_event(self, event: dict) -> None:
        """Routes binding calls of this instance from the Mapper tab.

        All instances share the control connection, so events are filtered by
        session and binding name.
        """
        if (
            event.get("sessionId") != self._mapper_session_id
            or event.get("method") != "Runtime.bindingCalled"
        ):
            return
        params = event.get("params", {})
        name = params.get("name")
        payload = params.get("payload", "")
        if name == self._send_bidi_binding:
            parsed = json.loads(payload)
            if parsed.get("type") == "event":
                self.received_events.append(parsed)
            self._bidi_queue.put_nowait(payload)
        elif name == self._send_cdp_binding and self._mapper_cdp_ws is not None:
            self._cdp_send_queue.put_nowait(payload)

    async def _forward_cdp_to_browser(self) -> None:
        """Sends CDP messages from the in-tab Mapper to the browser, in order.

        `task_done()` lets `close()` wait until all queued messages are sent.
        """
        assert self._mapper_cdp_ws is not None
        try:
            while True:
                payload = await self._cdp_send_queue.get()
                try:
                    await self._mapper_cdp_ws.send(payload)
                finally:
                    self._cdp_send_queue.task_done()
        except websockets.ConnectionClosed as e:
            # The WebSocket is expected to close after `close()`. Before that,
            # it is an error: report it to the test.
            if not self._closed:
                self._bidi_queue.put_nowait(e)
                raise
        except Exception as e:
            self._bidi_queue.put_nowait(e)
            raise

    async def _forward_cdp_from_browser(self) -> None:
        """Delivers CDP messages from the browser to the in-tab Mapper.

        Each message is passed to `onCdpMessage_<id>` in the Mapper tab. The
        next message is delivered only after the previous one, to keep order.
        """
        assert self._mapper_cdp_ws is not None
        try:
            async for raw_cdp_message in self._mapper_cdp_ws:
                parsed_cdp = json.loads(raw_cdp_message)
                # The Mapper attaches its browser session through the root
                # session of this WebSocket. So a `Target.detachedFromTarget`
                # without `sessionId` means the browser session was detached.
                if (
                    parsed_cdp.get("method") == "Target.detachedFromTarget"
                    and "sessionId" not in parsed_cdp
                ):
                    self._browser_session_detached.set()
                # `?.` makes this a no-op after the Mapper was closed.
                expression = (
                    f"window[{json.dumps(self._on_cdp_fn)}]?.("
                    f"{json.dumps(raw_cdp_message)})"
                )
                res = await self._control_cdp.send_command(
                    "Runtime.evaluate",
                    {"expression": expression},
                    session_id=self._mapper_session_id,
                )
                assert "exceptionDetails" not in res, res
        except websockets.ConnectionClosed as e:
            # The WebSocket is expected to close after `close()`. Before that,
            # it is an error: report it to the test.
            if not self._closed:
                self._bidi_queue.put_nowait(e)
                raise
        except Exception as e:
            self._bidi_queue.put_nowait(e)
            raise

    async def start(self, mapper_target_id: str, capabilities: dict) -> None:
        """Creates the transports in the Mapper tab, starts a Mapper instance
        with them and creates a BiDi session.
        """
        # Bindings let the in-tab Mapper send messages to the test: calling
        # `window.<binding>(payload)` emits `Runtime.bindingCalled` on the
        # control connection.
        await self._control_cdp.send_command(
            "Runtime.addBinding",
            {"name": self._send_bidi_binding},
            session_id=self._mapper_session_id,
        )
        if self._mapper_cdp_ws is not None:
            await self._control_cdp.send_command(
                "Runtime.addBinding",
                {"name": self._send_cdp_binding},
                session_id=self._mapper_session_id,
            )

        # The JS snippets below are combined into `init_expression`. They are
        # f-strings, so `{{` and `}}` are literal JS braces.
        if self._mapper_cdp_ws is not None:
            # Custom CDP transport (implements `Transport` from
            # `src/utils/transport.ts`).
            cdp_setup_js = f"""
                let onCdpMessage = null;
                const cdpTransport = {{
                    setOnMessage(handler) {{
                        onCdpMessage = handler;
                    }},
                    // Mapper -> browser: relayed by `_forward_cdp_to_browser`.
                    sendMessage(message) {{
                        if (!cdpClosed) {{
                            window[{json.dumps(self._send_cdp_binding)}](message);
                        }}
                    }},
                    close() {{
                        cdpClosed = true;
                        onCdpMessage = null;
                    }},
                }};
                // Browser -> Mapper: called by `_forward_cdp_from_browser`.
                window[{json.dumps(self._on_cdp_fn)}] = (messageStr) => {{
                    onCdpMessage?.(messageStr);
                }};
            """
            run_call_js = f"""
                await window.runMapperInstance(
                    {json.dumps(mapper_target_id)},
                    bidiTransport,
                    cdpTransport,
                )
            """
            # Used by `closeMapper_<id>` if `runMapperInstance` failed.
            fallback_close_js = "bidiTransport.close(); cdpTransport.close();"
        else:
            # No custom CDP transport: `runMapperInstance` uses the shared
            # default `window.cdp` transport.
            cdp_setup_js = ""
            run_call_js = f"""
                await window.runMapperInstance(
                    {json.dumps(mapper_target_id)},
                    bidiTransport,
                )
            """
            fallback_close_js = "bidiTransport.close();"

        init_expression = f"""
            (async () => {{
                // State of this instance, read by `closeMapper_<id>`.
                let onBidiMessage = null;
                let bidiClosed = false;
                let bidiCloseCount = 0;
                let bidiMessagesAfterClose = 0;
                let cdpClosed = false;
                // The started BidiServer, or null if it failed to start.
                let mapper = null;

                // Custom BiDi transport (implements `BidiTransport`).
                const bidiTransport = {{
                    setOnMessage(handler) {{
                        onBidiMessage = handler;
                    }},
                    // Mapper -> test: read by `_on_control_cdp_event`. Messages
                    // sent after close are counted and dropped.
                    sendMessage(message) {{
                        if (bidiClosed) {{
                            bidiMessagesAfterClose++;
                            return;
                        }}
                        window[{json.dumps(self._send_bidi_binding)}](
                            JSON.stringify(message),
                        );
                    }},
                    close() {{
                        bidiClosed = true;
                        bidiCloseCount++;
                    }},
                }};

                {cdp_setup_js}

                // Test -> Mapper: called by `send()`.
                window[{json.dumps(self._on_bidi_fn)}] = (messageStr) => {{
                    onBidiMessage?.(JSON.parse(messageStr));
                }};
                // Called once by `close()`. Closes the Mapper and returns the
                // transport state, which `close()` checks.
                window[{json.dumps(self._close_fn)}] = async () => {{
                    delete window[{json.dumps(self._on_bidi_fn)}];
                    delete window[{json.dumps(self._on_cdp_fn)}];
                    delete window[{json.dumps(self._close_fn)}];
                    const hadMapper = mapper !== null;
                    if (hadMapper) {{
                        // A command which is still processed when the Mapper
                        // closes. Its response must be dropped.
                        onBidiMessage?.({{
                            id: 999998,
                            method: 'session.status',
                            params: {{}},
                        }});
                        // Closing twice must close the transport only once.
                        mapper.close();
                        mapper.close();
                        // A command received after close must be ignored.
                        onBidiMessage?.({{
                            id: 999999,
                            method: 'session.status',
                            params: {{}},
                        }});
                        // Let the async command processing finish, so any late
                        // response is counted in `bidiMessagesAfterClose`.
                        await new Promise((resolve) => setTimeout(resolve, 0));
                    }} else {{
                        {fallback_close_js}
                    }}
                    return {{
                        hadMapper,
                        bidiClosed,
                        bidiCloseCount,
                        bidiMessagesAfterClose,
                        cdpClosed,
                    }};
                }};

                // Start the Mapper instance. The returned BidiServer is kept in
                // this closure for `closeMapper_<id>`.
                mapper = {run_call_js};
            }})()
        """
        res = await self._control_cdp.send_command(
            "Runtime.evaluate",
            {"expression": init_expression, "awaitPromise": True},
            session_id=self._mapper_session_id,
        )
        assert "exceptionDetails" not in res, res

        await execute_command(
            self,
            {
                "method": "session.new",
                "params": {"capabilities": {"alwaysMatch": capabilities}},
            },
        )

    async def send(self, message: str) -> None:
        """Sends a BiDi message to the in-tab Mapper. Used by `test_helpers`."""
        # `?.` makes this a no-op after the Mapper was closed.
        expression = f"window[{json.dumps(self._on_bidi_fn)}]?.({json.dumps(message)})"
        res = await self._control_cdp.send_command(
            "Runtime.evaluate",
            {"expression": expression},
            session_id=self._mapper_session_id,
        )
        assert "exceptionDetails" not in res, res

    async def recv(self) -> str:
        """Returns the next BiDi message from the in-tab Mapper. Used by
        `test_helpers`. Raises errors reported by the background tasks.
        """
        item = await self._bidi_queue.get()
        if isinstance(item, BaseException):
            raise item
        return item

    def _collect_bg_errors(self) -> list[BaseException]:
        """Returns the errors of the finished CDP forwarding tasks."""
        errors: list[BaseException] = []
        for task in (self._cdp_send_task, self._cdp_forward_task):
            if task is not None and task.done() and not task.cancelled():
                exc = task.exception()
                if exc is not None:
                    errors.append(exc)
        return errors

    async def close(self, *, close_ws: bool = True) -> None:
        """Closes the in-tab Mapper and checks how it closed its transports.

        Args:
            close_ws: If `False`, only the in-tab Mapper is closed. The custom
                CDP WebSocket stays open and this method waits until the
                browser confirms that the Mapper detached its browser session.
                This checks that the Mapper itself detaches the session, as
                closing the WebSocket would detach it anyway. The
                `create_mapper` fixture calls `close()` again at teardown to
                release the rest.
        """
        # Step 1: close the in-tab Mapper (only once).
        close_state = None
        if not self._mapper_closed:
            self._mapper_closed = True
            try:
                res = await self._control_cdp.send_command(
                    "Runtime.evaluate",
                    {
                        "expression": f"window[{json.dumps(self._close_fn)}]?.()",
                        "awaitPromise": True,
                        "returnByValue": True,
                    },
                    session_id=self._mapper_session_id,
                )
                assert "exceptionDetails" not in res, res
                close_state = res.get("result", {}).get("value")
            except (ConnectionError, websockets.ConnectionClosed):
                # The control connection is already closed, so is the tab.
                pass

            # Wait until the CDP messages sent by the Mapper while closing (e.g.
            # `Target.detachFromTarget`) are sent to the browser.
            if self._cdp_send_task is not None and not self._cdp_send_task.done():
                await asyncio.wait_for(self._cdp_send_queue.join(), timeout=5.0)

        if not close_ws:
            if (
                close_state is not None
                and close_state["hadMapper"]
                and self._mapper_cdp_ws is not None
            ):
                await asyncio.wait_for(
                    self._browser_session_detached.wait(), timeout=5.0
                )
            bg_errors = self._collect_bg_errors()
            if bg_errors:
                raise bg_errors[0]
            # See the same checks at the end of this method.
            if close_state is not None and close_state["hadMapper"]:
                assert close_state["bidiClosed"] is True
                assert close_state["bidiCloseCount"] == 1
                assert close_state["bidiMessagesAfterClose"] == 0
                if self._mapper_cdp_ws is not None:
                    assert close_state["cdpClosed"] is True
            return

        # Step 2: release the Python-side resources (only once).
        if self._closed:
            return
        self._closed = True

        self._control_cdp.off_event(self._on_control_cdp_event)

        # Collect errors before cancelling, so cancellation is not reported.
        bg_errors = self._collect_bg_errors()

        tasks_to_cancel = [
            t
            for t in (self._cdp_send_task, self._cdp_forward_task)
            if t is not None and not t.done()
        ]
        for task in tasks_to_cancel:
            task.cancel()
        if tasks_to_cancel:
            await asyncio.gather(*tasks_to_cancel, return_exceptions=True)

        if self._mapper_cdp_ws is not None:
            await self._mapper_cdp_ws.close()

        # The Mapper closed its BiDi transport exactly once, sent no BiDi
        # messages after that, and closed its custom CDP transport.
        if close_state is not None and close_state["hadMapper"]:
            assert close_state["bidiClosed"] is True
            assert close_state["bidiCloseCount"] == 1
            assert close_state["bidiMessagesAfterClose"] == 0
            if self._mapper_cdp_ws is not None:
                assert close_state["cdpClosed"] is True

        if bg_errors:
            raise bg_errors[0]


@pytest_asyncio.fixture
async def browser_ws_url(websocket) -> str:
    """Returns the browser's direct DevTools WebSocket URL."""
    # ChromeDriver does not expose the browser DevTools URL. Read it from the
    # `DevToolsActivePort` file in the browser's user data directory. Its first
    # line is the port, the second line is the browser target path.
    cmd_line = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {"method": "Browser.getBrowserCommandLine", "params": {}},
        },
    )
    user_data_dir = next(
        arg.split("=", 1)[1]
        for arg in cmd_line["result"]["arguments"]
        if arg.startswith("--user-data-dir=")
    )
    active_port_path = os.path.join(user_data_dir, "DevToolsActivePort")
    with open(active_port_path, encoding="utf-8") as f:
        port, browser_path = f.read().strip().splitlines()[:2]
    return f"ws://127.0.0.1:{port}{browser_path}"


@pytest_asyncio.fixture
async def mapper_tab_cdp(browser_ws_url, websocket):
    """Creates a new direct CDP connection to the browser and attaches to the
    BiDi-CDP Mapper tab.

    At teardown, fails the test if the Mapper tab has unhandled promise
    rejections, e.g. from fire-and-forget CDP commands which fail when mappers
    race with each other or are closed.
    """
    control_ws = await websockets.connect(browser_ws_url)
    control_cdp = _CdpConnection(control_ws)
    try:
        # Find the Mapper tab started by ChromeDriver and attach to it.
        mapper_target_id = await get_mapper_target_id(websocket)
        attached = await control_cdp.send_command(
            "Target.attachToTarget",
            {"targetId": mapper_target_id, "flatten": True},
        )
        mapper_session_id = attached["sessionId"]
        await control_cdp.send_command("Runtime.enable", session_id=mapper_session_id)
        # Record unhandled promise rejections in the Mapper tab.
        hook_res = await control_cdp.send_command(
            "Runtime.evaluate",
            {
                "expression": """
                    window.__unhandledRejections = [];
                    window.__onUnhandledRejection = (event) => {
                        window.__unhandledRejections.push(
                            String(event.reason?.message ?? event.reason),
                        );
                    };
                    window.addEventListener(
                        'unhandledrejection',
                        window.__onUnhandledRejection,
                    );
                """
            },
            session_id=mapper_session_id,
        )
        assert "exceptionDetails" not in hook_res, hook_res
        yield {
            "control_cdp": control_cdp,
            "mapper_target_id": mapper_target_id,
            "mapper_session_id": mapper_session_id,
        }
        # This runs after the test and after the `create_mapper` teardown
        # (which depends on this fixture), so mapper teardown is checked too.
        # `setTimeout` lets pending rejections be reported first.
        unhandled_res = await control_cdp.send_command(
            "Runtime.evaluate",
            {
                "expression": """
                    (async () => {
                        await new Promise((resolve) => setTimeout(resolve, 0));
                        return window.__unhandledRejections;
                    })()
                """,
                "awaitPromise": True,
                "returnByValue": True,
            },
            session_id=mapper_session_id,
        )
        assert "exceptionDetails" not in unhandled_res, unhandled_res
        assert unhandled_res["result"]["value"] == []
    finally:
        # Remove the hook, so it does not leak into the following tests, which
        # reuse the same browser.
        try:
            await control_cdp.send_command(
                "Runtime.evaluate",
                {
                    "expression": """
                        window.removeEventListener(
                            'unhandledrejection',
                            window.__onUnhandledRejection,
                        );
                        delete window.__onUnhandledRejection;
                        delete window.__unhandledRejections;
                    """
                },
                session_id=mapper_session_id,
            )
        except (ConnectionError, RuntimeError, websockets.ConnectionClosed):
            pass
        await control_cdp.close()


@pytest_asyncio.fixture
async def create_mapper(browser_ws_url, mapper_tab_cdp, capabilities):
    """Returns a factory that creates new BiDi (and by default CDP) transports
    in the Mapper tab and starts a new Mapper instance there.

    Factory args:
        custom_capabilities: Capabilities for `session.new`. Defaults to the
            `capabilities` fixture.
        custom_cdp_transport: If `True` (default), the Mapper gets its own CDP
            transport over a new browser WebSocket. If `False`, it shares the
            default `window.cdp` transport with the primary Mapper.

    All created mappers are closed at teardown.
    """
    control_cdp = mapper_tab_cdp["control_cdp"]
    mapper_target_id = mapper_tab_cdp["mapper_target_id"]
    mapper_session_id = mapper_tab_cdp["mapper_session_id"]

    mappers: list[InTabMapperConnection] = []
    counter = itertools.count(1)

    async def _create_mapper(
        custom_capabilities: dict | None = None,
        *,
        custom_cdp_transport: bool = True,
    ) -> InTabMapperConnection:
        instance_id = f"mapper_{next(counter)}"
        cdp_ws = (
            await websockets.connect(browser_ws_url) if custom_cdp_transport else None
        )
        mapper = InTabMapperConnection(
            control_cdp, cdp_ws, mapper_session_id, instance_id
        )
        # Register before `start()`, so a partly started Mapper is closed too.
        mappers.append(mapper)
        await mapper.start(
            mapper_target_id,
            capabilities if custom_capabilities is None else custom_capabilities,
        )
        return mapper

    try:
        yield _create_mapper
    finally:
        # Close all mappers (also the ones the test already closed, which is a
        # no-op or releases the rest), then raise the first error.
        results = await asyncio.gather(
            *(mapper.close() for mapper in reversed(mappers)),
            return_exceptions=True,
        )
        for res in results:
            if isinstance(res, BaseException):
                raise res


@pytest.mark.asyncio
async def test_multiple_mappers_browsing_context_get_tree(
    websocket, create_mapper, context_id
):
    """All mappers see the same browsing context tree, and a context created by
    one Mapper is reported to the others.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    # Initially, all mappers (including the primary one) see only the default
    # tab.
    tree_primary = await get_tree(websocket)
    tree_1 = await get_tree(mapper_1)
    tree_2 = await get_tree(mapper_2)

    assert [c["context"] for c in tree_primary["contexts"]] == [context_id]
    assert [c["context"] for c in tree_1["contexts"]] == [context_id]
    assert [c["context"] for c in tree_2["contexts"]] == [context_id]

    # `mapper_1` creates a tab. The primary Mapper and `mapper_2` are notified.
    await subscribe(websocket, ["browsingContext.contextCreated"])
    await subscribe(mapper_2, ["browsingContext.contextCreated"])

    create_res = await execute_command(
        mapper_1,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )
    second_context_id = create_res["context"]

    created_primary, created_2 = await asyncio.gather(
        wait_for_event(websocket, "browsingContext.contextCreated"),
        wait_for_event(mapper_2, "browsingContext.contextCreated"),
    )
    assert created_primary["params"]["context"] == second_context_id
    assert created_2["params"]["context"] == second_context_id

    # Now all mappers see both tabs.
    tree_primary_after = await get_tree(websocket)
    tree_1_after = await get_tree(mapper_1)
    tree_2_after = await get_tree(mapper_2)

    expected_contexts = {context_id, second_context_id}
    assert {c["context"] for c in tree_primary_after["contexts"]} == expected_contexts
    assert {c["context"] for c in tree_1_after["contexts"]} == expected_contexts
    assert {c["context"] for c in tree_2_after["contexts"]} == expected_contexts


@pytest.mark.asyncio
async def test_multiple_mappers_default_transports(
    websocket, mapper_tab_cdp, create_mapper, context_id, html
):
    """Checks the default transports of `runMapperInstance` and the cleanup:

    1. The default `WindowBidiTransport`, used by the primary Mapper, cannot be
       used by a second Mapper.
    2. If the Mapper fails to start, its browser session is detached and its
       custom CDP transport is closed.
    3. A Mapper without a custom CDP transport shares `window.cdp` with the
       primary Mapper, and both work.
    4. A closed Mapper detaches its browser session: its preload scripts are
       not applied to new tabs, and the primary Mapper keeps working.
    """
    control_cdp = mapper_tab_cdp["control_cdp"]
    mapper_target_id = mapper_tab_cdp["mapper_target_id"]
    mapper_session_id = mapper_tab_cdp["mapper_session_id"]

    # 1. Re-binding the singleton default WindowBidiTransport must fail rather
    # than overwriting the primary ChromeDriver Mapper's message handler.
    dup_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": f"window.runMapperInstance({json.dumps(mapper_target_id)})",
            "awaitPromise": True,
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" in dup_res
    assert "Default WindowBidiTransport is already in use" in dup_res["result"].get(
        "description", ""
    )

    # 2. If `BidiServer.createAndStart` rejects after `createBrowserSession`
    # succeeds on a custom CDP transport, `runMapperInstance` must detach that
    # browser session and close the custom CDP transport.
    failed_init_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": f"""
                (async () => {{
                    let onCdp = null;
                    let cdpClosed = false;
                    let detachedSessionId = null;
                    // Fake CDP transport, no browser involved:
                    // - `Target.attachToBrowserTarget` succeeds with session S1;
                    // - `Target.detachFromTarget` is recorded;
                    // - all other commands fail, so the Mapper fails to start
                    //   after its browser session was created.
                    const failingCdpTransport = {{
                        setOnMessage(handler) {{ onCdp = handler; }},
                        sendMessage(msg) {{
                            const {{id, method, params}} = JSON.parse(msg);
                            if (method === 'Target.attachToBrowserTarget') {{
                                // Reply asynchronously like a real transport.
                                // Like Chromium, send the attach event before
                                // the command result.
                                queueMicrotask(() => {{
                                    onCdp?.(JSON.stringify({{
                                        method: 'Target.attachedToTarget',
                                        params: {{sessionId: 'S1'}},
                                    }}));
                                    onCdp?.(JSON.stringify({{
                                        id,
                                        result: {{sessionId: 'S1'}},
                                    }}));
                                }});
                            }} else if (method === 'Target.detachFromTarget') {{
                                detachedSessionId = params.sessionId;
                            }} else {{
                                queueMicrotask(() => onCdp?.(JSON.stringify({{
                                    id,
                                    error: {{code: -32000, message: 'init failed'}},
                                }})));
                            }}
                        }},
                        close() {{ cdpClosed = true; }},
                    }};
                    const dummyBidiTransport = {{
                        setOnMessage() {{}},
                        sendMessage() {{}},
                        close() {{}},
                    }};
                    let errorMessage = null;
                    try {{
                        await window.runMapperInstance(
                            {json.dumps(mapper_target_id)},
                            dummyBidiTransport,
                            failingCdpTransport,
                        );
                    }} catch (err) {{
                        errorMessage = err?.message ?? String(err);
                    }}
                    return {{errorMessage, cdpClosed, detachedSessionId}};
                }})()
            """,
            "awaitPromise": True,
            "returnByValue": True,
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" not in failed_init_res, failed_init_res
    failed_init_val = failed_init_res["result"]["value"]
    assert failed_init_val["cdpClosed"] is True
    assert failed_init_val["detachedSessionId"] == "S1"
    assert (
        failed_init_val["errorMessage"] is not None
        and "init failed" in failed_init_val["errorMessage"]
    )

    # 3. Omitting `cdpTransport` reuses `windowCdpConnection` without clobbering
    # the primary ChromeDriver `websocket` session.
    secondary_mapper = await create_mapper(custom_cdp_transport=False)
    custom_cdp_mapper = await create_mapper(custom_cdp_transport=True)

    # `secondary_mapper` creates a tab: both it and the primary Mapper report
    # it, so both work over the shared `window.cdp`.
    await subscribe(websocket, ["browsingContext.contextCreated"])
    await subscribe(secondary_mapper, ["browsingContext.contextCreated"])

    create_cmd_id = await send_JSON_command(
        secondary_mapper,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )

    created_primary, (create_res, created_secondary) = await asyncio.gather(
        wait_for_event(websocket, "browsingContext.contextCreated"),
        wait_for_command_and_event(
            secondary_mapper, create_cmd_id, "browsingContext.contextCreated"
        ),
    )
    second_context_id = create_res["context"]
    assert created_primary["params"]["context"] == second_context_id
    assert created_secondary["params"]["context"] == second_context_id

    # 4. Register preload scripts on both secondary mappers before closing them;
    # if `close()` fails to detach their Browser CDP sessions, they would still
    # auto-attach to `third_context_id` and inject these globals on navigation.
    await execute_command(
        secondary_mapper,
        {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.__closedSharedMapper = true; }"
            },
        },
    )
    await execute_command(
        custom_cdp_mapper,
        {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.__closedCustomMapper = true; }"
            },
        },
    )

    # Closing secondary mappers detaches their Browser CDP sessions (even when
    # the underlying browser WebSocket stays open) and closes custom CDP
    # transports while leaving the primary `websocket` Mapper live.
    # `close_ws=False` keeps the custom CDP WebSocket open, so the preload
    # script check below fails if the Mapper does not detach its session.
    await secondary_mapper.close()
    await custom_cdp_mapper.close(close_ws=False)

    # The primary Mapper still works: it creates a new tab and navigates it.
    create_cmd_id = await send_JSON_command(
        websocket,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )
    third_res, created_after_close = await wait_for_command_and_event(
        websocket, create_cmd_id, "browsingContext.contextCreated"
    )
    third_context_id = third_res["context"]
    assert created_after_close["params"]["context"] == third_context_id

    await goto_url(websocket, third_context_id, html("<h1>After close</h1>"))
    # The preload scripts of the closed mappers did not run in the new tab.
    eval_after_close = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "[window.__closedSharedMapper, window.__closedCustomMapper]",
                "target": {"context": third_context_id},
                "awaitPromise": False,
            },
        },
    )
    assert eval_after_close["result"] == {
        "type": "array",
        "value": [{"type": "undefined"}, {"type": "undefined"}],
    }

    tree_primary = await get_tree(websocket)
    assert {c["context"] for c in tree_primary["contexts"]} == {
        context_id,
        second_context_id,
        third_context_id,
    }


@pytest.mark.asyncio
async def test_multiple_mappers_isolated_event_subscriptions(
    create_mapper, context_id, html
):
    """Subscriptions are per Mapper: each Mapper receives only the events it
    subscribed to.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    # Mapper 1 subscribes only to `browsingContext.load`.
    # Mapper 2 subscribes only to `log.entryAdded`.
    await subscribe(mapper_1, ["browsingContext.load"])
    await subscribe(mapper_2, ["log.entryAdded"])
    # Drop the events received during setup.
    mapper_1.received_events.clear()
    mapper_2.received_events.clear()

    # The page emits both events: `log.entryAdded` from the inline script, and
    # `browsingContext.load` when it finishes loading.
    url = html("<script>console.log('hello from page');</script><h1>Test</h1>")
    nav_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url, "context": context_id, "wait": "complete"},
        },
    )

    (_, load_event), log_event = await asyncio.gather(
        wait_for_command_and_event(mapper_1, nav_cmd_id, "browsingContext.load"),
        wait_for_event(mapper_2, "log.entryAdded"),
    )
    assert load_event == AnyExtending(
        {
            "type": "event",
            "method": "browsingContext.load",
            "params": {"context": context_id, "url": url},
        }
    )
    assert log_event == AnyExtending(
        {
            "type": "event",
            "method": "log.entryAdded",
            "params": {
                "text": "hello from page",
                "source": {"context": context_id},
                "timestamp": ANY_TIMESTAMP,
            },
        }
    )

    # Flush both mappers (including a renderer round-trip on `mapper_2` after
    # `navigate(wait="complete")` finished) so any leaked event would already
    # be recorded in `received_events`.
    await get_tree(mapper_1)
    await execute_command(
        mapper_2,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "1",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    # `received_events` also has the events dropped by the wait helpers above,
    # so this checks that no other event was received.
    assert [e["method"] for e in mapper_1.received_events] == ["browsingContext.load"]
    assert [e["method"] for e in mapper_2.received_events] == ["log.entryAdded"]


@pytest.mark.asyncio
async def test_multiple_mappers_actuate_and_observe(create_mapper, context_id, html):
    """One Mapper navigates the page while the other observes it and then
    interacts with it. Internal sandboxes of one Mapper are not exposed to the
    other.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await subscribe(
        mapper_1, ["browsingContext.navigationCommitted", "script.realmCreated"]
    )
    await subscribe(mapper_2, ["browsingContext.navigationCommitted"])
    # Drop the events received during setup.
    mapper_1.received_events.clear()

    url = html("<h1 id='title'>Agent Target</h1>")
    # Actuate navigation from Mapper 1 while Mapper 2 observes.
    nav_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url, "context": context_id, "wait": "complete"},
        },
    )

    (_, nav_1), nav_2 = await asyncio.gather(
        wait_for_command_and_event(
            mapper_1, nav_cmd_id, "browsingContext.navigationCommitted"
        ),
        wait_for_event(mapper_2, "browsingContext.navigationCommitted"),
    )
    assert nav_1["params"]["context"] == context_id
    assert nav_1["params"]["url"] == url
    assert nav_2["params"]["context"] == context_id
    assert nav_2["params"]["url"] == url

    # Mapper 2 locates nodes and captures screenshot on the navigated page.
    nodes_result = await execute_command(
        mapper_2,
        {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": context_id,
                "locator": {"type": "css", "value": "#title"},
            },
        },
    )
    assert len(nodes_result["nodes"]) == 1

    screenshot_result = await execute_command(
        mapper_2,
        {
            "method": "browsingContext.captureScreenshot",
            "params": {"context": context_id},
        },
    )
    assert screenshot_result == {"data": ANY_STR}

    # Internal helper sandboxes created by `captureScreenshot` on `mapper_2`
    # must stay hidden from sibling mappers (`script.getRealms` and
    # `script.realmCreated`). Flush `mapper_1` with a renderer round-trip first
    # so `Runtime.executionContextCreated` for the isolated world has arrived.
    await execute_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "1",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )
    # `mapper_1` sees only the page's default realm, and no sandbox realm was
    # reported to it.
    realms_1 = await execute_command(
        mapper_1,
        {"method": "script.getRealms", "params": {"context": context_id}},
    )
    assert len(realms_1["realms"]) == 1
    assert "sandbox" not in realms_1["realms"][0]
    assert all(
        "sandbox" not in e["params"]
        for e in mapper_1.received_events
        if e["method"] == "script.realmCreated"
    )


@pytest.mark.asyncio
async def test_multiple_mappers_script_evaluate_and_sandboxes(
    create_mapper, context_id, html
):
    """Both mappers can evaluate scripts in a sandbox with the same name in the
    same context.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await goto_url(mapper_1, context_id, html("<h1>Sandbox Test</h1>"))

    # Both mappers can evaluate scripts in a sandbox on the same context.
    # Note: whether same-named sandboxes across mappers share an isolated world
    # or are isolated per mapper is not specified here; this only checks that
    # `script.evaluate` with a `sandbox` succeeds on both mappers.
    res_1 = await execute_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.tag = 'mapper1'; window.tag",
                "target": {"context": context_id, "sandbox": "agent"},
                "awaitPromise": False,
            },
        },
    )
    res_2 = await execute_command(
        mapper_2,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.tag = 'mapper2'; window.tag",
                "target": {"context": context_id, "sandbox": "agent"},
                "awaitPromise": False,
            },
        },
    )
    assert res_1["result"] == {"type": "string", "value": "mapper1"}
    assert res_2["result"] == {"type": "string", "value": "mapper2"}


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "capabilities", [{"unhandledPromptBehavior": "ignore"}], indirect=True
)
async def test_multiple_mappers_user_prompt_handling(create_mapper, context_id):
    """With `unhandledPromptBehavior: ignore`, no Mapper closes prompts
    automatically. A prompt opened via one Mapper is reported to both mappers
    and can be closed via the other one.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await subscribe(
        mapper_1,
        ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
    )
    await subscribe(
        mapper_2,
        ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
    )

    # Open an alert via Mapper 1. Do not wait for the result: `alert()` blocks
    # until the prompt is closed.
    await send_JSON_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "alert('agent prompt')",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    opened_1, opened_2 = await asyncio.gather(
        wait_for_event(mapper_1, "browsingContext.userPromptOpened"),
        wait_for_event(mapper_2, "browsingContext.userPromptOpened"),
    )
    assert opened_1["params"]["message"] == "agent prompt"
    assert opened_2["params"]["message"] == "agent prompt"

    # Handle the prompt from Mapper 2 and verify both mappers observe closure.
    handle_cmd_id = await send_JSON_command(
        mapper_2,
        {
            "method": "browsingContext.handleUserPrompt",
            "params": {"context": context_id, "accept": True},
        },
    )

    closed_1, (_, closed_2) = await asyncio.gather(
        wait_for_event(mapper_1, "browsingContext.userPromptClosed"),
        wait_for_command_and_event(
            mapper_2, handle_cmd_id, "browsingContext.userPromptClosed"
        ),
    )

    assert closed_1["params"] == AnyExtending(
        {"context": context_id, "accepted": True, "type": "alert"}
    )
    assert closed_2["params"] == AnyExtending(
        {"context": context_id, "accepted": True, "type": "alert"}
    )


@pytest.mark.asyncio
async def test_multiple_mappers_auto_dismiss_user_prompt(
    websocket, create_mapper, context_id
):
    """With the default `unhandledPromptBehavior` (`dismiss` for alerts), every
    Mapper tries to dismiss the same alert. Only one
    `Page.handleJavaScriptDialog` succeeds; the others fail with "No dialog is
    showing". These failures must not cause unhandled promise rejections in the
    Mapper tab (checked at `mapper_tab_cdp` teardown).
    """
    # Three mappers race: the primary one, one with a custom CDP transport, and
    # one sharing `window.cdp` with the primary Mapper.
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper(custom_cdp_transport=False)

    for conn in (websocket, mapper_1, mapper_2):
        await subscribe(
            conn,
            ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
        )

    # `alert()` blocks until the alert is dismissed, so the `script.evaluate`
    # result arrives after the alert is closed.
    eval_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "alert('auto prompt')",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    async def _wait_opened_and_closed(conn) -> tuple[dict, dict]:
        """Reads messages until both prompt events arrive, in any order."""
        opened = None
        closed = None
        while opened is None or closed is None:
            msg = await read_JSON_message(conn)
            if msg.get("method") == "browsingContext.userPromptOpened":
                opened = msg
            elif msg.get("method") == "browsingContext.userPromptClosed":
                closed = msg
        return opened, closed

    async def _wait_mapper_1() -> tuple[dict, dict, dict]:
        """Same as `_wait_opened_and_closed`, but also waits for the
        `script.evaluate` result on `mapper_1`.
        """
        eval_res = None
        opened = None
        closed = None
        while eval_res is None or opened is None or closed is None:
            msg = await read_JSON_message(mapper_1)
            if msg.get("id") == eval_cmd_id:
                assert "result" in msg, msg
                eval_res = msg["result"]
            elif msg.get("method") == "browsingContext.userPromptOpened":
                opened = msg
            elif msg.get("method") == "browsingContext.userPromptClosed":
                closed = msg
        return eval_res, opened, closed

    (
        (opened_p, closed_p),
        (_, opened_1, closed_1),
        (opened_2, closed_2),
    ) = await asyncio.wait_for(
        asyncio.gather(
            _wait_opened_and_closed(websocket),
            _wait_mapper_1(),
            _wait_opened_and_closed(mapper_2),
        ),
        timeout=20,
    )

    # Every Mapper reports the alert with the `dismiss` handler, and its
    # closing as not accepted.
    for opened in (opened_p, opened_1, opened_2):
        assert opened["params"] == AnyExtending(
            {
                "context": context_id,
                "handler": "dismiss",
                "message": "auto prompt",
                "type": "alert",
            }
        )
    for closed in (closed_p, closed_1, closed_2):
        assert closed["params"] == AnyExtending(
            {"context": context_id, "accepted": False, "type": "alert"}
        )

    # Flush all three mappers so any losing `Page.handleJavaScriptDialog`
    # responses (`"No dialog is showing"`) settle before checking for unhandled
    # promise rejections in `mapper_tab_cdp` teardown.
    for conn in (websocket, mapper_1, mapper_2):
        await execute_command(
            conn,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": "1",
                    "target": {"context": context_id},
                    "awaitPromise": False,
                },
            },
        )


@pytest.mark.asyncio
async def test_multiple_mappers_no_cdp_errors(
    websocket,
    mapper_tab_cdp,
    create_mapper,
    html,
    iframe,
):
    """No CDP command fails while several mappers create a tab, attach to an
    out-of-process iframe (OOPIF), close the tab and are closed.

    CDP errors are collected from the debug log of the Mapper tab: all Mapper
    instances log received CDP messages via `window.sendDebugMessage`.
    """
    control_cdp = mapper_tab_cdp["control_cdp"]
    mapper_session_id = mapper_tab_cdp["mapper_session_id"]

    # Hook `window.sendDebugMessage` before starting secondary mappers so CDP
    # traffic during `runMapperInstance` and `session.new` is also checked.
    # The original hook (if any) is still called and restored in `finally`.
    hook_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": """
                window.cdpErrors = [];
                window.__origSendDebugMessage = window.sendDebugMessage;
                window.sendDebugMessage = (json) => {
                    window.__origSendDebugMessage?.(json);
                    const data = JSON.parse(json);
                    if (data.logType === 'cdp:RECV ◂' && data.messages?.[0]?.error) {
                        window.cdpErrors.push(data.messages[0]);
                    }
                };
            """
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" not in hook_res, hook_res

    try:
        # `mapper_1` has its own CDP transport, `mapper_2` shares `window.cdp`
        # with the primary Mapper.
        mapper_1 = await create_mapper()
        mapper_2 = await create_mapper(custom_cdp_transport=False)

        await subscribe(websocket, ["browsingContext"])
        await subscribe(mapper_1, ["browsingContext", "script.realmCreated"])
        await subscribe(mapper_2, ["browsingContext", "script.realmCreated"])

        # `mapper_1` creates a tab. Wait until the other mappers attached to it.
        create_res = await execute_command(
            mapper_1,
            {"method": "browsingContext.create", "params": {"type": "tab"}},
        )
        new_context_id = create_res["context"]
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["context"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_2,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["context"] == new_context_id
                ),
            ),
        )

        # `mapper_2` loads a page with a cross-origin iframe, which becomes a
        # separate OOPIF target. Wait until the other mappers attached to it.
        oopif_url = html("<h1>OOPIF</h1>", same_origin=False)
        await goto_url(mapper_2, new_context_id, html(iframe(oopif_url)))
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["parent"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_1,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["parent"] == new_context_id
                ),
            ),
        )

        tree_1 = await get_tree(mapper_1, new_context_id)
        tree_2 = await get_tree(mapper_2, new_context_id)
        assert len(tree_1["contexts"][0]["children"]) == 1
        assert len(tree_2["contexts"][0]["children"]) == 1
        child_context_id = tree_1["contexts"][0]["children"][0]["context"]

        # Ensure all mappers (`websocket`, `mapper_1`, and `mapper_2`) have
        # finished unblocking the OOPIF target before `mapper_2` closes the tab
        # so in-flight `#unblock()` CDP commands are not aborted mid-attachment.
        for conn in (websocket, mapper_1, mapper_2):
            await execute_command(
                conn,
                {
                    "method": "script.evaluate",
                    "params": {
                        "expression": "1 + 1",
                        "target": {"context": child_context_id},
                        "awaitPromise": False,
                    },
                },
            )

        # `mapper_2` closes the tab. Wait until the other mappers report it.
        await execute_command(
            mapper_2,
            {"method": "browsingContext.close", "params": {"context": new_context_id}},
        )
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextDestroyed"
                    and e["params"]["context"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_1,
                lambda e: (
                    e["method"] == "browsingContext.contextDestroyed"
                    and e["params"]["context"] == new_context_id
                ),
            ),
        )

        # Also close both secondary mappers (and flush `windowCdpConnection` via
        # `websocket`) before inspecting `window.cdpErrors` so CDP traffic
        # during mapper teardown is verified error-free.
        await mapper_1.close()
        await mapper_2.close()
        await execute_command(
            websocket,
            {
                "method": "goog:cdp.sendCommand",
                "params": {"method": "Browser.getVersion", "params": {}},
            },
        )

        errors_result = await control_cdp.send_command(
            "Runtime.evaluate",
            {"expression": "window.cdpErrors", "returnByValue": True},
            session_id=mapper_session_id,
        )
        assert errors_result["result"]["value"] == []
    finally:
        await control_cdp.send_command(
            "Runtime.evaluate",
            {
                "expression": """
                    window.sendDebugMessage = window.__origSendDebugMessage;
                    delete window.__origSendDebugMessage;
                """
            },
            session_id=mapper_session_id,
        )
