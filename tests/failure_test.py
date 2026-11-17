"""Offline terminal failure and cleanup scenarios; never submit a REAL prompt."""
import os
import pty
import select
import signal
import socket
import threading
import subprocess
import sys
import tempfile
import termios
import time

binary = os.path.abspath(sys.argv[1])
scenario = sys.argv[2]

class Terminal:
    def __init__(self, directory, real=False):
        self.master, self.slave = pty.openpty()
        self.initial = termios.tcgetattr(self.slave)
        environment = os.environ.copy()
        environment.pop('COG_TUI_TEST_MISSING_REFERENCE', None)
        environment['COG_TUI_TEST_PRESENT_REFERENCE'] = 'fabricated-offline-test-key'
        args = [binary, '--config-dir', directory]
        if not real:
            args.append('--mock')
        self.process = subprocess.Popen(args, stdin=self.slave, stdout=self.slave,
                                        stderr=self.slave, env=environment)
        self.output = bytearray()

    def until(self, needle, timeout=5):
        end = time.monotonic() + timeout
        while needle not in self.output:
            if time.monotonic() > end:
                raise AssertionError(bytes(self.output))
            if select.select([self.master], [], [], .05)[0]:
                self.output.extend(os.read(self.master, 65536))
            if self.process.poll() is not None and needle not in self.output:
                raise AssertionError(bytes(self.output))

    def send(self, text):
        os.write(self.master, text)

    def finish(self, expected=0):
        result = self.process.wait(timeout=5)
        assert result == expected, (result, bytes(self.output))
        assert termios.tcgetattr(self.slave) == self.initial

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        if self.master >= 0:
            os.close(self.master)
        os.close(self.slave)

with tempfile.TemporaryDirectory(prefix='failure-', dir=os.getcwd()) as directory:
    path = os.path.join(directory, 'settings')
    initial = ('version=1\nprovider=openrouter\nmodel=fixture/model\n'
               'base_url=https://openrouter.ai/api/v1\n'
               'credential_ref=env:COG_TUI_TEST_MISSING_REFERENCE\n')
    listener = None
    if scenario == 'provider-failure':
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(5)
        port = listener.getsockname()[1]
        initial = initial.replace('https://openrouter.ai/api/v1', 'https://127.0.0.1:' + str(port))
        initial = initial.replace('COG_TUI_TEST_MISSING_REFERENCE', 'COG_TUI_TEST_PRESENT_REFERENCE')
        def reject_tls():
            try:
                connection, _ = listener.accept()
                connection.close()
            except (OSError, TimeoutError):
                pass
        threading.Thread(target=reject_tls, daemon=True).start()
    with open(path, 'w', encoding='utf-8') as file:
        file.write(initial)
    os.chmod(path, 0o600)
    if scenario == 'first-run':
        os.unlink(path)
    terminal = Terminal(directory, real=scenario in ('credential-recovery', 'provider-failure', 'first-run'))
    try:
        if scenario == 'first-run':
            terminal.until(b'Model:')
            terminal.send(b'fixture/model\n\nenv:COG_TUI_TEST_PRESENT_REFERENCE\n')
            terminal.until(b'Ready. Provider: openrouter.')
            assert 'model=fixture/model' in open(path).read()
            terminal.send(b'/quit\n')
            terminal.finish()
        elif scenario == 'credential-recovery':
            terminal.until(b'Runtime setup failed')
            terminal.send(b'/settings\n')
            terminal.until(b'Model:')
            terminal.send(b'\n\nenv:COG_TUI_TEST_PRESENT_REFERENCE\n')
            terminal.until(b'Ready. Provider: openrouter.')
            assert b'fabricated-offline-test-key' not in terminal.output
            assert 'fabricated-offline-test-key' not in open(path).read()
            terminal.output.clear()
            terminal.send(b'/settings\n')
            terminal.until(b'Model:')
            terminal.send(b'\n\nenv:COG_TUI_TEST_MISSING_REFERENCE\n')
            terminal.until(b'Apply saved settings failed')
            terminal.send(b'/quit\n')
            terminal.finish()
        elif scenario == 'provider-failure':
            terminal.until(b'Ready. Provider: openrouter.')
            terminal.send(b'offline loopback failure\n')
            terminal.until(b'Turn incomplete or failed.')
            terminal.finish(expected=1)
        else:
            terminal.until(b'MOCK ready')
            if scenario == 'eof':
                terminal.send(b'\x04')
                terminal.finish()
            elif scenario == 'sigint-idle':
                terminal.process.send_signal(signal.SIGINT)
                terminal.finish()
            elif scenario in ('sigterm-active', 'sigint-active'):
                terminal.send(b'hello\n')
                terminal.until(b'assistant: Hello ')
                terminal.process.send_signal(signal.SIGTERM if scenario == 'sigterm-active' else signal.SIGINT)
                if scenario == 'sigint-active':
                    terminal.until(b'Turn cancelled.')
                    terminal.send(b'/quit\n')
                terminal.finish()
            elif scenario == 'output-hangup':
                os.close(terminal.master)
                terminal.master = -1
                # The lost terminal makes restoration/output fail; exit must be bounded.
                assert terminal.process.wait(timeout=5) != 0
            elif scenario == 'save-retry':
                os.chmod(directory, 0o500)
                terminal.send(b'/settings\n')
                terminal.until(b'Model:')
                terminal.send(b'changed/model\n\n\n')
                terminal.until(b'Settings save failed')
                assert open(path).read() == initial
                os.chmod(directory, 0o700)
                terminal.send(b'\n')
                terminal.until(b'Settings saved.')
                assert 'model=changed/model\n' in open(path).read()
                terminal.send(b'/quit\n')
                terminal.finish()
            else:
                raise AssertionError(scenario)
    finally:
        os.chmod(directory, 0o700)
        terminal.close()
        if listener is not None:
            listener.close()
print(scenario + ': PASS')
