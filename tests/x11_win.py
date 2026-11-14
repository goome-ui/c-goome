# x11_win.py, helpers for the window part of tests/x11.sh
#   close WID             sends WM_DELETE_WINDOW like a close button
#   drop WID X Y PATH...  drags files onto WID at X,Y (client coords) over xdnd, prints the status it got
#   cursor                prints w h xhot yhot and a sum of the pixels of the cursor on screen
#   dpi N                 sets Xft.dpi in RESOURCE_MANAGER on the root
import ctypes, sys, time
x = ctypes.CDLL("libX11.so.6")
V, U, I = ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int
x.XOpenDisplay.restype = V
x.XDefaultRootWindow.restype = U; x.XDefaultRootWindow.argtypes = [V]
x.XInternAtom.restype = U; x.XInternAtom.argtypes = [V, ctypes.c_char_p, I]
x.XSendEvent.argtypes = [V, U, I, ctypes.c_long, V]
x.XSync.argtypes = [V, I]; x.XFlush.argtypes = [V]
x.XCreateSimpleWindow.restype = U; x.XCreateSimpleWindow.argtypes = [V, U, I, I, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, U, U]
x.XSetSelectionOwner.argtypes = [V, U, U, U]
x.XNextEvent.argtypes = [V, V]; x.XPending.argtypes = [V]
x.XChangeProperty.argtypes = [V, U, U, U, I, I, ctypes.c_char_p, I]
x.XTranslateCoordinates.argtypes = [V, U, U, I, I, ctypes.POINTER(I), ctypes.POINTER(I), ctypes.POINTER(U)]
d = x.XOpenDisplay(None); root = x.XDefaultRootWindow(d)
A = lambda n: x.XInternAtom(d, n.encode(), 0)

def client(win, typ, l):
    ev = (ctypes.c_long * 24)()
    b = ctypes.cast(ev, ctypes.POINTER(ctypes.c_ulong))
    ctypes.cast(ev, ctypes.POINTER(I))[0] = 33
    b[4], b[5] = win, A(typ)
    ctypes.cast(ctypes.byref(ev, 48), ctypes.POINTER(I))[0] = 32
    for i, v in enumerate(l): b[7 + i] = v
    x.XSendEvent(d, win, 0, 0, ev)
    x.XFlush(d)

def events(src, until, secs=2):
    end = time.time() + secs
    while time.time() < end:
        while x.XPending(d):
            ev = (ctypes.c_long * 24)()
            x.XNextEvent(d, ev)
            b = ctypes.cast(ev, ctypes.POINTER(ctypes.c_ulong))
            r = until(ctypes.cast(ev, ctypes.POINTER(I))[0], b)
            if r is not None: return r
        time.sleep(0.01)
    return None

cmd = sys.argv[1]
if cmd == "close":
    client(int(sys.argv[2], 0), "WM_PROTOCOLS", [A("WM_DELETE_WINDOW"), 0])
elif cmd == "dpi":
    s = ("Xft.dpi:\t%s\n" % sys.argv[2]).encode()
    x.XChangeProperty(d, root, A("RESOURCE_MANAGER"), A("STRING"), 8, 0, s, len(s))
    x.XSync(d, 0)
elif cmd == "cursor":
    f = ctypes.CDLL("libXfixes.so.3")
    class Img(ctypes.Structure):
        _fields_ = [("x", ctypes.c_short), ("y", ctypes.c_short), ("w", ctypes.c_ushort), ("h", ctypes.c_ushort), ("xhot", ctypes.c_ushort),
                    ("yhot", ctypes.c_ushort), ("serial", U), ("px", ctypes.POINTER(U)), ("atom", U), ("name", ctypes.c_char_p)]
    f.XFixesGetCursorImage.restype = ctypes.POINTER(Img); f.XFixesGetCursorImage.argtypes = [V]
    c = f.XFixesGetCursorImage(d).contents
    print(c.w, c.h, c.xhot, c.yhot, sum(c.px[i] & 0xffffffff for i in range(c.w * c.h)))
elif cmd == "drop":
    win, cx, cy = int(sys.argv[2], 0), int(sys.argv[3]), int(sys.argv[4])
    data = "".join("file://" + "".join(c if (c.isascii() and (c.isalnum() or c in "/._-")) else "".join("%%%02X" % b for b in c.encode()) for c in p) + "\r\n" for p in sys.argv[5:]).encode()
    src = x.XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0)
    x.XSetSelectionOwner(d, A("XdndSelection"), src, 0)
    rx, ry, ch = I(), I(), U()
    x.XTranslateCoordinates(d, win, root, cx, cy, ctypes.byref(rx), ctypes.byref(ry), ctypes.byref(ch))
    client(win, "XdndEnter", [src, 5 << 24, A("text/uri-list")])
    client(win, "XdndPosition", [src, 0, (rx.value << 16) | ry.value, 0, A("XdndActionCopy")])
    st = events(src, lambda t, b: (b[7], b[8]) if t == 33 and b[5] == A("XdndStatus") else None)
    print("status", "none" if st is None else "accept %d" % (st[1] & 1))
    client(win, "XdndDrop", [src, 0, 0])
    def serve(t, b):
        if t == 30:
            req, sel, tgt, prop = b[5], b[6], b[7], b[8]
            x.XChangeProperty(d, req, prop, tgt, 8, 0, data, len(data))
            ev = (ctypes.c_long * 24)(); e = ctypes.cast(ev, ctypes.POINTER(ctypes.c_ulong))
            ctypes.cast(ev, ctypes.POINTER(I))[0] = 31
            e[4], e[5], e[6], e[7] = req, sel, tgt, prop
            x.XSendEvent(d, req, 0, 0, ev); x.XFlush(d)
        if t == 33 and b[5] == A("XdndFinished"): return b[8]
    fin = events(src, serve, 3)
    print("finished", "none" if fin is None else fin & 1)
