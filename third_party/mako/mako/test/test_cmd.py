from contextlib import contextmanager
import os
from unittest import mock

from mako.cmd import cmdline
from mako.testing.assertions import eq_
from mako.testing.assertions import expect_raises
from mako.testing.assertions import expect_raises_message
from mako.testing.config import config
from mako.testing.fixtures import TemplateTest


class CmdTest(TemplateTest):
    @contextmanager
    def _capture_output_fixture(self, stream="stdout"):
        with mock.patch("sys.%s" % stream) as stdout:
            yield stdout

    def test_stdin_success(self):
        with self._capture_output_fixture() as stdout:
            with mock.patch(
                "sys.stdin",
                mock.Mock(read=mock.Mock(return_value="hello world ${x}")),
            ):
                cmdline(["--var", "x=5", "-"])

        eq_(stdout.write.mock_calls[0][1][0], "hello world 5")

    def test_stdin_syntax_err(self):
        with mock.patch(
            "sys.stdin", mock.Mock(read=mock.Mock(return_value="${x"))
        ):
            with self._capture_output_fixture("stderr") as stderr:
                with expect_raises(SystemExit):
                    cmdline(["--var", "x=5", "-"])

            assert (
                "SyntaxException: Expected" in stderr.write.mock_calls[0][1][0]
            )
            assert "Traceback" in stderr.write.mock_calls[0][1][0]

    def test_stdin_rt_err(self):
        with mock.patch(
            "sys.stdin", mock.Mock(read=mock.Mock(return_value="${q}"))
        ):
            with self._capture_output_fixture("stderr") as stderr:
                with expect_raises(SystemExit):
                    cmdline(["--var", "x=5", "-"])

            assert "NameError: Undefined" in stderr.write.mock_calls[0][1][0]
            assert "Traceback" in stderr.write.mock_calls[0][1][0]

    def test_file_success(self):
        with self._capture_output_fixture() as stdout:
            cmdline(
                [
                    "--var",
                    "x=5",
                    os.path.join(config.template_base, "cmd_good.mako"),
                ]
            )

        eq_(stdout.write.mock_calls[0][1][0], "hello world 5")

    def test_file_syntax_err(self):
        with self._capture_output_fixture("stderr") as stderr:
            with expect_raises(SystemExit):
                cmdline(
                    [
                        "--var",
                        "x=5",
                        os.path.join(config.template_base, "cmd_syntax.mako"),
                    ]
                )

        assert "SyntaxException: Expected" in stderr.write.mock_calls[0][1][0]
        assert "Traceback" in stderr.write.mock_calls[0][1][0]

    def test_file_rt_err(self):
        with self._capture_output_fixture("stderr") as stderr:
            with expect_raises(SystemExit):
                cmdline(
                    [
                        "--var",
                        "x=5",
                        os.path.join(config.template_base, "cmd_runtime.mako"),
                    ]
                )

        assert "NameError: Undefined" in stderr.write.mock_calls[0][1][0]
        assert "Traceback" in stderr.write.mock_calls[0][1][0]

    def test_file_notfound(self):
        with expect_raises_message(
            SystemExit, "error: can't find fake.lalala"
        ):
            cmdline(["--var", "x=5", "fake.lalala"])
