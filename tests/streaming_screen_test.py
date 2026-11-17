"""Verify actual terminal cells with libvterm, not just captured escape bytes."""
import ctypes as c
import ctypes.util
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

library = ctypes.util.find_library("vterm")
if not library:
    print("libvterm is unavailable; screen emulation check skipped")
    sys.exit(77)
vt = c.CDLL(library)


class Rect(c.Structure):
    _fields_ = [(name, c.c_int) for name in ("top", "bottom", "left", "right")]


class Pos(c.Structure):
    _fields_ = [("row", c.c_int), ("col", c.c_int)]


def api(name, args, result=None):
    fn = getattr(vt, name)
    fn.argtypes, fn.restype = args, result
    return fn


new = api("vterm_new", [c.c_int, c.c_int], c.c_void_p)
free = api("vterm_free", [c.c_void_p])
screen_for = api("vterm_obtain_screen", [c.c_void_p], c.c_void_p)
state_for = api("vterm_obtain_state", [c.c_void_p], c.c_void_p)
reset = api("vterm_screen_reset", [c.c_void_p, c.c_int])
feed = api("vterm_input_write", [c.c_void_p, c.c_char_p, c.c_size_t], c.c_size_t)
get_text = api("vterm_screen_get_text", [c.c_void_p, c.c_void_p, c.c_size_t, Rect], c.c_size_t)
resize = api("vterm_set_size", [c.c_void_p, c.c_int, c.c_int])
cursor = api("vterm_state_get_cursorpos", [c.c_void_p, c.POINTER(Pos)])

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="screen-", dir=os.getcwd()) as directory:
    rows, columns = 12, 40
    terminal = new(rows, columns)
    screen = screen_for(terminal)
    reset(screen, 1)
    master, slave = pty.openpty()
    original = termios.tcgetattr(slave)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
    process = subprocess.Popen([binary, "--mock", "--config-dir", directory],
                               stdin=slave, stdout=slave, stderr=slave,
                               env=dict(os.environ, TERM="xterm"))
    captured = bytearray()

    def lines():
        result = []
        for row in range(rows):
            buffer = c.create_string_buffer(columns * 6 + 1)
            n = get_text(screen, buffer, len(buffer), Rect(row, row + 1, 0, columns))
            result.append(buffer.raw[:n].decode("utf-8").rstrip())
        return result

    def pump(duration=.02):
        if select.select([master], [], [], duration)[0]:
            block = os.read(master, 65536)
            captured.extend(block)
            feed(terminal, block, len(block))

    def until(predicate):
        end = time.monotonic() + 5
        while not predicate(lines()):
            assert process.poll() is None, bytes(captured)
            assert time.monotonic() < end, "\n".join(lines())
            pump()

    def settle(duration=.15):
        end = time.monotonic() + duration
        while time.monotonic() < end:
            pump()

    def separated():
        display = lines()
        assert display[-2] == "--- input ---", display
        assert all("cog>" not in line for line in display[:-1]), display
        position = Pos()
        cursor(state_for(terminal), c.byref(position))
        assert position.row == rows - 1, (position.row, display)

    try:
        until(lambda s: any("MOCK ready" in line for line in s))
        os.write(master, b"hello\n")
        until(lambda s: any("assistant: Hello " in line + " " for line in s))
        os.write(master, b"draft\x1b[D\x1b[D")
        until(lambda s: "assistant: Hello from cog-cli!" in s)
        settle()
        separated()
        assert sum(line.count("assistant:") for line in lines()) == 1, lines()
        assert lines()[-1] == "cog> draft", lines()
        os.write(master, b"X")
        until(lambda s: s[-1] == "cog> draXft")
        rows, columns = 10, 26
        resize(terminal, rows, columns)
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
        process.send_signal(signal.SIGWINCH)
        until(lambda s: s[-2] == "--- input ---" and s[-1] == "cog> draXft")
        separated()
        os.write(master, b"Y\x05\n")
        until(lambda s: any("you: draXYft" in line for line in s))
        settle(.4)
        # Repeated wrapped messages force output scrolling; the prompt stays put.
        for number in range(4):
            os.write(master, f"turn-{number} with a long wrapping input\n".encode())
            settle(.45)
            separated()
            assert "assistant: Hello from cog-cli!" in "".join(lines()[:-2]), lines()
        # A draft wider than the screen must stay on its one input row.
        os.write(master, b"z" * 100)
        settle()
        separated()
        os.write(master, b"\x15/settings\n")
        until(lambda s: s[-1] == "Model:")
        os.write(master, b"screen-model\n\n\n")
        until(lambda s: s[-1] == "cog>" and any("Settings saved." in line for line in s))
        separated()
        os.write(master, b"/quit\n")
        assert process.wait(timeout=5) == 0
        assert termios.tcgetattr(slave) == original
        while select.select([master], [], [], .02)[0]:
            pump(0)
        assert b"\x1b[r" in captured[-100:], "scrolling region was not restored"
        print("Continuous streaming, draft/cursor, resize, scrolling and cleanup: PASS")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)
        os.close(slave)
        free(terminal)
