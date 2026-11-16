#!/usr/bin/env python3
import sys, time
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, GLib

def find_app(name, wait=15):
    end = time.time() + wait
    while time.time() < end:
        desk = Atspi.get_desktop(0)
        for i in range(desk.get_child_count()):
            a = desk.get_child_at_index(i)
            if a and a.get_name() == name:
                return a
        time.sleep(0.3)
    return None

def walk(o, depth, out):
    role = o.get_role_name()
    name = o.get_name()
    val = ""
    v = o.get_value_iface()
    if v:
        val = " value=%g min=%g max=%g" % (Atspi.Value.get_current_value(o), Atspi.Value.get_minimum_value(o), Atspi.Value.get_maximum_value(o))
    t = o.get_text_iface()
    if t and role in ("entry", "text", "password text"):
        val += " text=%r" % Atspi.Text.get_text(o, 0, -1)
    st = o.get_state_set()
    flags = [s for s in ("focusable", "checked", "expandable", "expanded", "selected", "editable") if st.contains(getattr(Atspi.StateType, s.upper()))]
    out.append((depth, role, name, val, flags, o))
    print("  " * depth + "%s %r%s %s" % (role, name, val, ",".join(flags)))
    for i in range(o.get_child_count()):
        c = o.get_child_at_index(i)
        if c:
            walk(c, depth + 1, out)

def pick(nodes, role, name):
    for d, r, n, v, f, o in nodes:
        if (r == role or (role == "button" and r == "push button")) and n == name:
            return o
    return None

def main():
    title = sys.argv[1] if len(sys.argv) > 1 else "goome a11y"
    app = find_app(title)
    if not app:
        print("FAIL no app called %r on the a11y bus" % title)
        return 1
    print("app %r toolkit %r" % (app.get_name(), app.get_toolkit_name()))
    nodes = []
    walk(app, 0, nodes)
    bad = 0
    def need(cond, what):
        nonlocal bad
        print(("ok   " if cond else "FAIL ") + what)
        bad += not cond
    btn = pick(nodes, "button", "Press me")
    chk = pick(nodes, "check box", "Dark mode")
    sld = pick(nodes, "slider", "Volume")
    ent = pick(nodes, "entry", "Name")
    cmb = pick(nodes, "combo box", "Fruit")
    need(pick(nodes, "frame", title) is not None, "frame named after the window")
    need(pick(nodes, "label", "Screen reader test") is not None, "label text")
    need(btn is not None, "button by name")
    need(chk is not None, "check box by name")
    need(sld is not None and abs(Atspi.Value.get_current_value(sld) - 3) < 1e-9 and Atspi.Value.get_maximum_value(sld) == 10, "slider value 3 of 0..10")
    need(ent is not None and Atspi.Text.get_text(ent, 0, -1) == "Bo", "entry text")
    need(cmb is not None and cmb.get_name() == "Fruit", "combo box")
    got = []
    def heard(ev):
        got.append((ev.type, ev.source.get_name() if ev.source else "", ev.detail1))
    ear = Atspi.EventListener.new(heard)
    for t in ("object:state-changed:focused", "object:state-changed:checked", "object:property-change:accessible-value", "object:text-changed:insert"):
        ear.register(t)
    def spin(ms):
        loop = GLib.MainLoop()
        GLib.timeout_add(ms, loop.quit)
        loop.run()
    if ent:
        need(Atspi.Component.grab_focus(ent), "focus the entry")
        spin(800)
        need(any(t.startswith("object:state-changed:focused") and n == "Name" and d == 1 for t, n, d in got), "focus event for the entry")
    if btn:
        e = btn.get_extents(Atspi.CoordType.WINDOW)
        need(e.width > 10 and e.height > 10, "button has a size %dx%d" % (e.width, e.height))
        need(btn.get_action_iface() is not None and Atspi.Action.get_n_actions(btn) >= 1 and Atspi.Action.do_action(btn, 0), "press the button")
    if chk:
        Atspi.Action.do_action(chk, 0)
    if sld:
        need(Atspi.Value.set_current_value(sld, 7.5), "set the slider")
    if ent:
        need(Atspi.EditableText.set_text_contents(ent, "Robin"), "set the entry")
    if cmb:
        Atspi.Action.do_action(cmb, 0)
    spin(1000)
    need(any(t.startswith("object:property-change:accessible-value") and n == "Volume" for t, n, d in got), "value event for the slider")
    need(any(t.startswith("object:state-changed:checked") and n == "Dark mode" and d == 1 for t, n, d in got), "checked event for the check box")
    need(any(t.startswith("object:text-changed:insert") and n == "Name" for t, n, d in got), "text event for the entry")
    if chk:
        need(chk.get_state_set().contains(Atspi.StateType.CHECKED), "check box is checked now")
    if sld:
        need(abs(Atspi.Value.get_current_value(sld) - 7.5) < 1e-9, "slider reads 7.5 back")
    if ent:
        need(Atspi.Text.get_text(ent, 0, -1) == "Robin", "entry reads Robin back")
    if cmb:
        nodes = []
        walk(app, 0, nodes)
        fig = pick(nodes, "list item", "Fig")
        need(fig is not None, "combo list is open with Fig in it")
        if fig:
            Atspi.Action.do_action(fig, 0)
    q = pick(nodes, "button", "Quit")
    if q:
        time.sleep(0.5)
        Atspi.Action.do_action(q, 0)
    return 1 if bad else 0

sys.exit(main())
