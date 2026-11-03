#!/usr/bin/env python3
# fake org.freedesktop.portal.ScreenCast on a private bus, so the portal code runs with no popup.
# frames come from a real mutter screencast on the real session bus (REAL_BUS), the pipewire fd is a plain
# connection to the real pipewire socket. FAKE_MODE=ok|deny, FAKE_CLOSE_AFTER=secs sends Session.Closed once.
# every call gets logged as one json line to FAKE_LOG.
import json, os, socket
import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib

XML = """<node>
<interface name="org.freedesktop.portal.ScreenCast">
 <method name="CreateSession"><arg type="a{sv}" direction="in"/><arg type="o" direction="out"/></method>
 <method name="SelectSources"><arg type="o" direction="in"/><arg type="a{sv}" direction="in"/><arg type="o" direction="out"/></method>
 <method name="Start"><arg type="o" direction="in"/><arg type="s" direction="in"/><arg type="a{sv}" direction="in"/><arg type="o" direction="out"/></method>
 <method name="OpenPipeWireRemote"><arg type="o" direction="in"/><arg type="a{sv}" direction="in"/><arg type="h" direction="out"/></method>
 <property name="version" type="u" access="read"/>
 <property name="AvailableCursorModes" type="u" access="read"/>
 <property name="AvailableSourceTypes" type="u" access="read"/>
</interface>
<interface name="org.freedesktop.portal.Session">
 <method name="Close"/>
 <signal name="Closed"><arg type="a{sv}"/></signal>
</interface>
</node>"""

node = Gio.DBusNodeInfo.new_for_xml(XML)
mode = os.environ.get("FAKE_MODE", "ok")
close_after = int(os.environ.get("FAKE_CLOSE_AFTER", "0"))
logf = open(os.environ["FAKE_LOG"], "a")
real = Gio.DBusConnection.new_for_address_sync(os.environ["REAL_BUS"],
    Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
sessions = {}
mutter = {"session": None}


def log(what, **kw):
    kw["call"] = what
    logf.write(json.dumps(kw) + "\n")
    logf.flush()


def sender_part(sender):
    return sender[1:].replace(".", "_")


def respond(path, code, results):
    def go():
        bus.emit_signal(None, path, "org.freedesktop.portal.Request", "Response", GLib.Variant("(ua{sv})", (code, results)))
        return False
    GLib.timeout_add(50, go)


def mutter_start():
    ms = "org.gnome.Mutter.ScreenCast"
    r = real.call_sync(ms, "/org/gnome/Mutter/ScreenCast", ms, "CreateSession", GLib.Variant("(a{sv})", ({},)), None, 0, 5000, None)
    sp = r.unpack()[0]
    r = real.call_sync(ms, sp, ms + ".Session", "RecordMonitor",
                       GLib.Variant("(sa{sv})", ("Virtual-1", {"cursor-mode": GLib.Variant("u", 0)})), None, 0, 5000, None)
    stp = r.unpack()[0]
    got = {}
    sub = real.signal_subscribe(ms, ms + ".Stream", "PipeWireStreamAdded", stp, None, 0,
                                lambda c, s, p, i, n, params: got.setdefault("node", params.unpack()[0]))
    real.call_sync(ms, sp, ms + ".Session", "Start", None, None, 0, 5000, None)
    ctx = GLib.MainContext.default()
    for _ in range(500):
        if "node" in got:
            break
        ctx.iteration(True)
    real.signal_unsubscribe(sub)
    r = real.call_sync(ms, stp, "org.freedesktop.DBus.Properties", "Get",
                       GLib.Variant("(ss)", (ms + ".Stream", "Parameters")), None, 0, 5000, None)
    params = r.unpack()[0]
    mutter["session"] = sp
    return got["node"], params.get("position", (0, 0)), params.get("size", (0, 0))


def mutter_stop():
    if mutter["session"]:
        try:
            real.call_sync("org.gnome.Mutter.ScreenCast", mutter["session"], "org.gnome.Mutter.ScreenCast.Session", "Stop",
                           None, None, 0, 2000, None)
        except GLib.Error:
            pass
    mutter["session"] = None


def send_closed(path):
    if path in sessions:
        log("EmitClosed", session=path)
        bus.emit_signal(None, path, "org.freedesktop.portal.Session", "Closed", GLib.Variant("(a{sv})", ({},)))
    return False


def on_call(conn, sender, path, iface, method, params, inv):
    global close_after
    a = params.unpack()
    if iface == "org.freedesktop.portal.Session" and method == "Close":
        log("Close", session=path)
        mutter_stop()
        sessions.pop(path, None)
        inv.return_value(None)
        return
    if method == "CreateSession":
        opts = a[0]
        req = "/org/freedesktop/portal/desktop/request/%s/%s" % (sender_part(sender), opts["handle_token"])
        sp = "/org/freedesktop/portal/desktop/session/%s/%s" % (sender_part(sender), opts["session_handle_token"])
        log(method, opts=opts, req=req)
        sessions[sp] = conn.register_object(sp, node.interfaces[1], on_call, None, None)
        inv.return_value(GLib.Variant("(o)", (req,)))
        respond(req, 0, {"session_handle": GLib.Variant("s", sp)})
    elif method == "SelectSources":
        sp, opts = a
        req = "/org/freedesktop/portal/desktop/request/%s/%s" % (sender_part(sender), opts["handle_token"])
        log(method, session=sp, opts=opts)
        inv.return_value(GLib.Variant("(o)", (req,)))
        respond(req, 0, {})
    elif method == "Start":
        sp, parent, opts = a
        req = "/org/freedesktop/portal/desktop/request/%s/%s" % (sender_part(sender), opts["handle_token"])
        log(method, session=sp, parent=parent, opts=opts)
        inv.return_value(GLib.Variant("(o)", (req,)))
        if mode == "deny":
            respond(req, 1, {})
            return
        nodeid, pos, size = mutter_start()
        streams = [(nodeid, {"position": GLib.Variant("(ii)", tuple(pos)), "size": GLib.Variant("(ii)", tuple(size)),
                             "source_type": GLib.Variant("u", 1)})]
        respond(req, 0, {"streams": GLib.Variant("a(ua{sv})", streams),
                         "restore_token": GLib.Variant("s", "0f1e2d3c-aaaa-4bbb-8ccc-123456789abc")})
        if close_after:
            GLib.timeout_add_seconds(close_after, send_closed, sp)
            close_after = 0
    elif method == "OpenPipeWireRemote":
        log(method, session=a[0])
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(os.path.join(os.environ["REAL_RUNTIME"], "pipewire-0"))
        fds = Gio.UnixFDList.new_from_array([os.dup(s.fileno())])
        s.close()
        inv.return_value_with_unix_fd_list(GLib.Variant("(h)", (0,)), fds)


def on_prop(conn, sender, path, iface, name):
    log("Get", name=name)
    return GLib.Variant("u", {"version": 5, "AvailableCursorModes": 7, "AvailableSourceTypes": 3}[name])


def on_name(conn, name):
    print("ready", flush=True)


conn = Gio.DBusConnection.new_for_address_sync(os.environ["DBUS_SESSION_BUS_ADDRESS"],
    Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
bus = conn
conn.register_object("/org/freedesktop/portal/desktop", node.interfaces[0], on_call, on_prop, None)
Gio.bus_own_name_on_connection(conn, "org.freedesktop.portal.Desktop", 0, on_name, None)
try:
    GLib.MainLoop().run()
finally:
    mutter_stop()
