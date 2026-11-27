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

"""Fixtures for running additional BiDi Mapper instances inside the Mapper tab.

Setup:

* The primary Mapper is started by ChromeDriver in the "BiDi-CDP Mapper" tab
  with `window.runMapperInstance(selfTargetId)`. It uses the default transports:
  `WindowBidiTransport` (`window.onBidiMessage` / `window.sendBidiResponse`)
  and `WindowCdpTransport` (`window.cdp`). The `websocket` fixture talks to it.
* Tests can start more Mapper instances in the same tab via `create_mapper`,
  which calls `window.runMapperInstance(selfTargetId, bidiTransport,
  cdpTransport?, restrictedMode?, aclHandler?)` and returns an
  `InTabMapperConnection`:

      test (Python)
        |
        |  control CDP WebSocket (`_CdpConnection`), attached to the Mapper tab
        |    BiDi in:   Runtime.evaluate("onBidiMessage_<id>(message)")
        |    BiDi out:  Runtime.bindingCalled("sendBidiResponse_<id>")
        |    ACL out:   Runtime.bindingCalled("setAclRequest_<id>")
        |    ACL in:    Runtime.evaluate("resolveAclRequest_<id>(id, resp)")
        |               Runtime.evaluate("notifyAclChanged_<id>(acl, reason)")
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
import inspect
import itertools
import json
import os
from collections.abc import Callable
from typing import Any

import pytest_asyncio
import websockets
from test_helpers import execute_command, get_mapper_target_id


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
    the BiDi-CDP Mapper tab with its own isolated BiDi (and optional CDP and
    embedder ACL) transport.

    Message flow (see the module docstring for the overview):

    * BiDi, test -> Mapper: `send()` evaluates `onBidiMessage_<id>(message)` in
      the Mapper tab over the shared control CDP connection.
    * BiDi, Mapper -> test: the Mapper calls the `sendBidiResponse_<id>`
      binding. The control connection receives `Runtime.bindingCalled`, and
      `_on_control_cdp_event` puts the message into `_bidi_queue`, which is
      read by `recv()`.
    * ACL (only with `acl_handler`): the Mapper calls `aclHandler.setAcl`,
      which invokes the `setAclRequest_<id>` binding. `_handle_set_acl` calls
      the Python callback and delivers the result back via
      `resolveAclRequest_<id>`. Browser-initiated updates are pushed via
      `notify_acl_changed()`, which evaluates `notifyAclChanged_<id>` to invoke
      the listener registered with `aclHandler.setOnAclChanged`.
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
        acl_handler: Callable[[dict, str | None], Any] | None = None,
    ):
        # CDP connection attached to the Mapper tab. Shared by all instances.
        self._control_cdp = control_cdp
        # Browser WebSocket for the custom CDP transport, or `None` to use the
        # default `window.cdp` transport.
        self._mapper_cdp_ws = mapper_cdp_ws
        # CDP session of `_control_cdp` attached to the Mapper tab.
        self._mapper_session_id = mapper_session_id
        self._instance_id = instance_id
        # Optional Python embedder callback for `aclHandler.setAcl(acl, reason)`.
        self._acl_handler = acl_handler
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
        self._acl_tasks: set[asyncio.Task[None]] = set()
        self._bg_errors: list[BaseException] = []
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
        self._set_acl_binding = f"setAclRequest_{instance_id}"
        # Functions called by the test (test -> in-tab):
        self._on_bidi_fn = f"onBidiMessage_{instance_id}"
        self._on_cdp_fn = f"onCdpMessage_{instance_id}"
        self._resolve_acl_fn = f"resolveAclRequest_{instance_id}"
        self._notify_acl_changed_fn = f"notifyAclChanged_{instance_id}"
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
        elif (
            name == self._set_acl_binding
            and self._acl_handler is not None
            and not self._closed
            and not self._mapper_closed
        ):
            data = json.loads(payload)
            task = asyncio.create_task(
                self._handle_set_acl(data["requestId"], data["acl"], data.get("reason"))
            )
            self._acl_tasks.add(task)
            task.add_done_callback(self._on_acl_task_done)

    def _on_acl_task_done(self, task: asyncio.Task[None]) -> None:
        if task not in self._acl_tasks:
            return
        self._acl_tasks.discard(task)
        if not task.cancelled():
            exc = task.exception()
            if exc is not None:
                self._bg_errors.append(exc)

    async def _handle_set_acl(
        self, request_id: int, acl: dict, reason: str | None
    ) -> None:
        """Runs the Python `acl_handler` callback and delivers its decision to
        the pending JS Promise in the Mapper tab.
        """
        assert self._acl_handler is not None
        if self._closed or self._mapper_closed:
            return
        try:
            result = self._acl_handler(acl, reason)
            if inspect.isawaitable(result):
                result = await result

            if self._closed or self._mapper_closed:
                return
            expression = (
                f"window[{json.dumps(self._resolve_acl_fn)}]?.("
                f"{json.dumps(request_id)}, {json.dumps(json.dumps(result))})"
            )
            res = await self._control_cdp.send_command(
                "Runtime.evaluate",
                {"expression": expression},
                session_id=self._mapper_session_id,
            )
            assert "exceptionDetails" not in res, res
        except asyncio.CancelledError:
            raise
        except (ConnectionError, websockets.ConnectionClosed) as e:
            if not self._closed:
                self._bidi_queue.put_nowait(e)
                raise
        except Exception as e:
            self._bidi_queue.put_nowait(e)
            raise

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

    async def start(
        self,
        mapper_target_id: str,
        capabilities: dict | None,
        *,
        restricted_mode: bool | None = None,
    ) -> None:
        """Creates the transports in the Mapper tab, starts a Mapper instance
        with them and creates a BiDi session when `capabilities` is provided.
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
        if self._acl_handler is not None:
            await self._control_cdp.send_command(
                "Runtime.addBinding",
                {"name": self._set_acl_binding},
                session_id=self._mapper_session_id,
            )

        if self._acl_handler is not None:
            restricted_arg = (
                json.dumps(restricted_mode)
                if restricted_mode is not None
                else "undefined"
            )
            extra_args_js = f", {restricted_arg}, aclHandler"
            acl_setup_js = f"""
                const pendingAclRequests = new Map();
                let nextAclRequestId = 1;
                let onAclChanged = null;
                const aclHandler = {{
                    setAcl(acl, reason) {{
                        const requestId = nextAclRequestId++;
                        return new Promise((resolve) => {{
                            pendingAclRequests.set(requestId, resolve);
                            window[{json.dumps(self._set_acl_binding)}](
                                JSON.stringify({{requestId, acl, reason}}),
                            );
                        }});
                    }},
                    setOnAclChanged(listener) {{
                        onAclChanged = listener;
                    }},
                }};
                window[{json.dumps(self._resolve_acl_fn)}] = (
                    requestId,
                    resultJson,
                ) => {{
                    const resolve = pendingAclRequests.get(requestId);
                    if (!resolve) {{
                        return;
                    }}
                    pendingAclRequests.delete(requestId);
                    resolve(JSON.parse(resultJson));
                }};
                window[{json.dumps(self._notify_acl_changed_fn)}] = (
                    aclJson,
                    reason,
                ) => {{
                    onAclChanged?.(JSON.parse(aclJson), reason);
                }};
            """
            acl_cleanup_js = f"""
                delete window[{json.dumps(self._resolve_acl_fn)}];
                delete window[{json.dumps(self._notify_acl_changed_fn)}];
                pendingAclRequests.clear();
            """
        else:
            extra_args_js = (
                f", {json.dumps(restricted_mode)}"
                if restricted_mode is not None
                else ""
            )
            acl_setup_js = ""
            acl_cleanup_js = ""

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
                    cdpTransport{extra_args_js},
                )
            """
            # Used by `closeMapper_<id>` if `runMapperInstance` failed.
            fallback_close_js = "bidiTransport.close(); cdpTransport.close();"
        else:
            # No custom CDP transport: pass `undefined`, so `runMapperInstance`
            # uses the shared default `window.cdp` transport.
            cdp_setup_js = ""
            run_call_js = f"""
                await window.runMapperInstance(
                    {json.dumps(mapper_target_id)},
                    bidiTransport,
                    undefined{extra_args_js},
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
                {acl_setup_js}

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
                    {acl_cleanup_js}
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

        if capabilities is not None:
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

    async def notify_acl_changed(self, acl: dict, reason: str | None = None) -> None:
        """Invokes the in-tab `aclHandler.setOnAclChanged` listener."""
        assert self._acl_handler is not None, (
            "notify_acl_changed requires an acl_handler"
        )
        # Pass `undefined` when omitted so JS sees no `reason` argument.
        reason_arg = "undefined" if reason is None else json.dumps(reason)
        expression = (
            f"window[{json.dumps(self._notify_acl_changed_fn)}]?.("
            f"{json.dumps(json.dumps(acl))}, {reason_arg})"
        )
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
        """Returns the errors of the finished background tasks."""
        for task in list(self._acl_tasks):
            if task.done():
                self._on_acl_task_done(task)
        errors: list[BaseException] = list(self._bg_errors)
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
            for t in (self._cdp_send_task, self._cdp_forward_task, *self._acl_tasks)
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
        restricted_mode: If set, passed as `restrictedMode` to
            `runMapperInstance`.
        acl_handler: Optional sync or async Python callback
            `(acl: dict, reason: str | None) -> dict` bridged to
            `aclHandler.setAcl` in `runMapperInstance`.
        create_session: If `True` (default), sends `session.new` after the
            Mapper starts.

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
        restricted_mode: bool | None = None,
        acl_handler: Callable[[dict, str | None], Any] | None = None,
        create_session: bool = True,
    ) -> InTabMapperConnection:
        instance_id = f"mapper_{next(counter)}"
        cdp_ws = (
            await websockets.connect(browser_ws_url) if custom_cdp_transport else None
        )
        mapper = InTabMapperConnection(
            control_cdp,
            cdp_ws,
            mapper_session_id,
            instance_id,
            acl_handler=acl_handler,
        )
        # Register before `start()`, so a partly started Mapper is closed too.
        mappers.append(mapper)
        session_capabilities = (
            None
            if not create_session
            else (capabilities if custom_capabilities is None else custom_capabilities)
        )
        await mapper.start(
            mapper_target_id,
            session_capabilities,
            restricted_mode=restricted_mode,
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
