# Catch the Caterina bootloader the instant its port appears and send the
# AVR109 chip-erase command ('e'). With page 0 erased the bootloader stays
# put on the next boot, so a normal upload can follow.
import os, sys, glob, time, termios, tty, select, subprocess
HEX = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build', 'bhoreal_apps.hex')
A = os.path.expanduser('~/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17')
end = time.time() + 900
erased = False
while time.time() < end:
    ports = glob.glob('/dev/cu.usbmodem*')
    if not ports:
        time.sleep(0.005); continue
    p = ports[0]
    t0 = time.time()
    if not erased:
        try:
            fd = os.open(p, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            tty.setraw(fd)
            a = termios.tcgetattr(fd); a[4] = a[5] = termios.B57600; termios.tcsetattr(fd, termios.TCSANOW, a)
            os.write(fd, b'e')
            got = b''
            while time.time() - t0 < 3 and b'\r' not in got:
                r, _, _ = select.select([fd], [], [], 0.05)
                if r: got += os.read(fd, 16)
            os.write(fd, b'E')  # leave bootloader is harmless if it already left
            os.close(fd)
            print(f"{p}: erase reply {got!r} after {time.time()-t0:.2f}s", flush=True)
            if b'\r' in got:
                erased = True
        except OSError as e:
            print(f"{p}: {e} after {time.time()-t0:.3f}s", flush=True)
        while os.path.exists(p): time.sleep(0.01)
        continue
    # Erased: the bootloader should now stay; flash normally.
    time.sleep(0.5)
    r = subprocess.run([A + '/bin/avrdude', '-C', A + '/etc/avrdude.conf', '-p', 'atmega32u4',
                        '-c', 'avr109', '-P', p, '-b', '57600', '-D', '-U', f'flash:w:{HEX}:i'],
                       capture_output=True, text=True)
    lines = [l for l in r.stderr.splitlines() if 'bytes of flash' in l or 'error' in l.lower()]
    print('\n'.join(lines[:6]), flush=True)
    if any('verified' in l for l in lines):
        print('RESCUED', flush=True); sys.exit(0)
    while os.path.exists(p): time.sleep(0.01)
print('timeout', flush=True)
