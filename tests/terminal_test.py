import os, pty, select, subprocess, sys, tempfile, time
binary=os.path.abspath(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='terminal-',dir=os.getcwd()) as directory:
    master,slave=pty.openpty()
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
    os.write(master,b'hello\n')
    until(b'Hello from cog-cli!')
    os.write(master,b'/settings\n')
    until(b'Model:')
    os.write(master,b'mock-next\n\n\n')
    until(b'Settings saved.')
    os.write(master,b'/quit\n')
    assert process.wait(timeout=5)==0,bytes(output)
    os.close(master)
    assert os.stat(directory+'/settings').st_mode & 0o777 == 0o600
    master,slave=pty.openpty()
    process=subprocess.Popen([binary,'--mock','--config-dir',directory],stdin=slave,stdout=slave,stderr=slave)
    os.close(slave);output.clear();until(b'MOCK ready')
    os.write(master,b'/show\n');until(b'model=mock-next')
    os.write(master,b'/quit\n');assert process.wait(timeout=5)==0,bytes(output);os.close(master)
print('terminal setup, conversation, save, restore: PASS')
