#!/opt/homebrew/Cellar/esptool/5.1.0/libexec/bin/python
"""Reset the ESP32 and print its serial log. Usage: scripts/serial_log.py [seconds=8] [port]
Uses esptool's bundled Python (it has pyserial). Reset happens via RTS."""
import glob, sys, time, serial
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 8
port = sys.argv[2] if len(sys.argv) > 2 else (glob.glob('/dev/cu.usbserial-*') or [None])[0]
s = serial.Serial(port, 115200, timeout=1)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
t = time.time()
while time.time() - t < secs:
    l = s.readline()
    if l: print(l.decode(errors='replace').rstrip())
