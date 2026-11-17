import os, pty, select, subprocess, sys, tempfile, time, termios, fcntl, struct, signal
binary=os.path.abspath(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='terminal-',dir=os.getcwd()) as directory:
    master,slave=pty.openpty()
    original_termios=termios.tcgetattr(master)
    process=subprocess.Popen([binary,'--mock','--config-dir',directory],stdin=slave,stdout=slave,stderr=slave)
    os.close(slave)
    output=bytearray()
    def until(needle,timeout=5):
        end=time.monotonic()+timeout
        while needle not in output:
            if time.monotonic()>end: raise AssertionError(bytes(output))
            if select.select([master],[],[],.1)[0]:
                try: block=os.read(master,65536)
                except OSError: block=b''
                if not block: raise AssertionError(bytes(output))
                output.extend(block)
    until(b'MOCK ready')
    os.write(master,b'hel')
    fcntl.ioctl(master,termios.TIOCSWINSZ,struct.pack('HHHH',30,100,0,0))
    process.send_signal(signal.SIGWINCH)
    os.write(master,b'lo\n')
    until(b'assistant: Hello ')
    os.write(master,b'/can')
    time.sleep(.03)
    os.write(master,b'cel\n')
    until(b'Turn cancelled.')
    output.clear()
    os.write(master,b'continue\n')
    until(b'assistant: from cog-cli!')
    time.sleep(.2)
    os.write(master,b'third turn\n')
    until(b'assistant: Hello ')
    time.sleep(.4)
    os.write(master,b'/settings\n')
    until(b'Model:')
    os.write(master,b'mock-next\n\n\n')
    until(b'Settings saved.')
    os.write(master,b'/quit\n')
    assert process.wait(timeout=5)==0,bytes(output)
    assert termios.tcgetattr(master)==original_termios
    os.close(master)
    assert os.stat(directory+'/settings').st_mode & 0o777 == 0o600
    master,slave=pty.openpty()
    process=subprocess.Popen([binary,'--mock','--config-dir',directory],stdin=slave,stdout=slave,stderr=slave)
    os.close(slave);output.clear();until(b'MOCK ready')
    os.write(master,b'/show\n');until(b'model=mock-next')
    os.write(master,b'/quit\n');assert process.wait(timeout=5)==0,bytes(output);os.close(master)
print('terminal setup, conversation, save, restore: PASS')
