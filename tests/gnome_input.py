import os, re, shlex, subprocess, sys, threading, time
from gi.repository import Gio, GLib

bus = Gio.bus_get_sync(Gio.BusType.SESSION)
RD, SC = 'org.gnome.Mutter.RemoteDesktop', 'org.gnome.Mutter.ScreenCast'


def call(dest, path, iface, method, args=None):
    return bus.call_sync(dest, path, iface, method, args, None, 0, 5000, None)


conn = call('org.gnome.Mutter.DisplayConfig', '/org/gnome/Mutter/DisplayConfig', 'org.gnome.Mutter.DisplayConfig', 'GetCurrentState').unpack()[1][0][0][0]
rd = call(RD, '/org/gnome/Mutter/RemoteDesktop', RD, 'CreateSession').unpack()[0]
rid = call(RD, rd, 'org.freedesktop.DBus.Properties', 'Get', GLib.Variant('(ss)', (RD + '.Session', 'SessionId'))).unpack()[0]
sc = call(SC, '/org/gnome/Mutter/ScreenCast', SC, 'CreateSession', GLib.Variant('(a{sv})', ({'remote-desktop-session-id': GLib.Variant('s', rid)},))).unpack()[0]
stream = call(SC, sc, SC + '.Session', 'RecordMonitor', GLib.Variant('(sa{sv})', (conn, {'cursor-mode': GLib.Variant('u', 0)}))).unpack()[0]
up = threading.Event()
bus.signal_subscribe(SC, SC + '.Stream', 'PipeWireStreamAdded', stream, None, 0, lambda *a: up.set())
threading.Thread(target=GLib.MainLoop().run, daemon=True).start()
call(RD, rd, RD + '.Session', 'Start')
up.wait(5)


def send(method, sig, *args):
    call(RD, rd, RD + '.Session', method, GLib.Variant(sig, args))


def key(sym):
    send('NotifyKeyboardKeysym', '(ub)', sym, True)
    time.sleep(0.05)
    send('NotifyKeyboardKeysym', '(ub)', sym, False)


key(0xffe1)
send('NotifyPointerMotionRelative', '(dd)', 1.0, 0.0)
time.sleep(0.4)
procs = []
for cmd in sys.argv[1:]:
    op, _, arg = cmd.partition(':')
    if op == 'run':
        out, _, line = arg.partition(':')
        procs.append(subprocess.Popen(shlex.split(line), stdout=open(out, 'w'), stderr=subprocess.STDOUT))
    elif op == 'wait':
        path, _, pat = arg.partition(':')
        end = time.time() + 5
        while time.time() < end and not any(re.search(pat, l) for l in open(path, errors='replace')):
            time.sleep(0.05)
    elif op == 'move':
        x, y = arg.split(',')
        send('NotifyPointerMotionAbsolute', '(sdd)', stream, float(x), float(y))
    elif op == 'click':
        send('NotifyPointerButton', '(ib)', 0x110, True)
        time.sleep(0.05)
        send('NotifyPointerButton', '(ib)', 0x110, False)
    elif op == 'key':
        key(int(arg, 0))
    elif op == 'sleep':
        time.sleep(float(arg))
    time.sleep(0.1)
for p in procs:
    p.terminate()
    p.wait()
call(RD, rd, RD + '.Session', 'Stop')
