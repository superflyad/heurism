#!/usr/bin/env python3
"""Companion local shell: native widgets, local Unix API, no root required."""
import json
import queue
import socket
import threading
import time
import tkinter as tk
from tkinter import messagebox
from pathlib import Path
import os
import re
import subprocess
from applications import Applications, atomic_write

SOCKET = '/run/companion-desktop/control.sock'


def request(action='status', value=None):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(20)
        client.connect(SOCKET)
        client.sendall(json.dumps({'version': 1, 'action': action, 'value': value}).encode() + b'\n')
        with client.makefile('rb') as stream:
            response = json.loads(stream.readline(65537))
    if not response.get('ok'):
        raise ValueError(response.get('error', 'Control service unavailable'))
    return response['data']


class Shell:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title('Companion')
        self.root.attributes('-type', 'desktop')
        self.root.geometry(f'{self.root.winfo_screenwidth()}x{self.root.winfo_screenheight()}+0+0')
        self.root.configure(bg='#0b1018')
        self.canvas = tk.Canvas(self.root, highlightthickness=0)
        self.canvas.pack(fill='both', expand=True)
        self.events = queue.Queue()
        self.commands = queue.Queue()
        self.data = None
        self.error = None
        self.notice = 'Connecting to the local control service'
        self.page = 'workspace'
        self.apps = Applications(self)
        self.buttons = []
        self.focus_index = -1
        self.pressed = None
        self.pointer_start = None
        self.ink = []
        self.scroll_position = 0
        self.pointer_count = 0
        self.input_state = None
        self.virtual_pointer_initialized = False
        self.panel = tk.Toplevel(self.root)
        self.panel.title('Companion panel')
        self.panel.attributes('-type', 'dock')
        self.panel.attributes('-topmost', True)
        self.panel.configure(bg='#111c2a')
        self.panel_height = 68
        self.panel_width = min(self.root.winfo_screenwidth()-48, 1040)
        self.panel_x = (self.root.winfo_screenwidth()-self.panel_width)//2
        self.panel_y = self.root.winfo_screenheight()-self.panel_height-16
        self.panel.geometry(f'{self.panel_width}x{self.panel_height}+{self.panel_x}+{self.panel_y}')
        self.panel.protocol('WM_DELETE_WINDOW', lambda: None)
        self.panel_left = tk.Frame(self.panel, bg='#111c2a')
        self.panel_left.pack(side='left', fill='y', padx=(12, 0))
        self.panel_tasks = tk.Frame(self.panel, bg='#111c2a')
        self.panel_tasks.pack(side='left', fill='both', expand=True, padx=12)
        self.panel_right = tk.Frame(self.panel, bg='#111c2a')
        self.panel_right.pack(side='right', fill='y', padx=(0, 14))
        self.panel_button('Companion', self.open_menu, self.panel_left, accent=True)
        for label, action in (('Files', 'files'), ('Editor', 'editor'), ('Browser', 'browser'),
                              ('Terminal', 'terminal')):
            self.panel_button(label, lambda name=action: self.launch(name), self.panel_left)
        self.panel_status = tk.Label(self.panel_right, bg='#111c2a', fg='#9db0c5',
                                     font=('DejaVu Sans', 10))
        self.panel_status.pack(side='left', padx=8)
        self.panel_clock = tk.Label(self.panel_right, bg='#111c2a', fg='#f0f5ff',
                                    font=('DejaVu Sans', 13, 'bold'))
        self.panel_clock.pack(side='left', padx=8)
        self.panel_menu = tk.Menu(self.panel, tearoff=False, bg='#17283a', fg='#f0f5ff',
                                  activebackground='#50e1be', activeforeground='#09251f',
                                  font=('DejaVu Sans', 12), bd=0, relief='flat')
        for label, callback in (('Desktop', lambda: self.show('workspace')),
                                ('System overview', lambda: self.show('overview')),
                                ('Input settings', lambda: self.show('input')),
                                ('Device information', lambda: self.show('device')),
                                ('Network', lambda: self.launch('network')),
                                ('Sound', lambda: self.launch('sound')),
                                ('On-screen keyboard', lambda: self.launch('keyboard')),
                                ('Switch applications', self.apps.switcher),
                                ('Administrator terminal', self.administrative_console),
                                ('Restart', lambda: self.power('reboot')),
                                ('Shut down', lambda: self.power('poweroff'))):
            self.panel_menu.add_command(label=label, command=callback)
        self.task_signature = None
        self.root.bind('<Configure>', lambda e: self.draw())
        self.root.bind('<F5>', lambda e: self.commands.put(('status', None)))
        self.root.bind('<Control-q>', lambda e: self.root.destroy())
        self.root.bind('<F1>', lambda e: self.show('overview'))
        self.root.bind('<F2>', lambda e: self.show('input'))
        self.root.bind('<F3>', lambda e: self.show('device'))
        self.root.bind('<F4>', lambda e: self.show('workspace'))
        self.root.bind('<Escape>', lambda e: self.show('workspace'))
        self.root.bind('<Tab>', self.focus_next)
        self.root.bind('<Return>', self.activate)
        self.root.bind('<space>', self.activate)
        self.canvas.bind('<ButtonPress-1>', self.press)
        self.canvas.bind('<ButtonRelease-1>', self.release)
        self.canvas.bind('<B1-Motion>', self.drag)
        self.canvas.bind('<Motion>', self.hover)
        self.canvas.bind('<Button-4>', lambda e: self.scroll(-1))
        self.canvas.bind('<Button-5>', lambda e: self.scroll(1))
        self.canvas.bind('<MouseWheel>', lambda e: self.scroll(-1 if e.delta > 0 else 1))
        threading.Thread(target=self.worker, daemon=True).start()
        self.root.after(100, self.tick)
        self.root.after(250, self.root.focus_force)
        self.root.after(500, self.reserve_panel_space)

    def worker(self):
        while True:
            try:
                try:
                    action, value = self.commands.get(timeout=2)
                except queue.Empty:
                    action, value = 'status', None
                if action != 'status':
                    result = request(action, value)
                    self.events.put(('notice', f"{action.capitalize()} updated"))
                self.events.put(('state', request()))
                self.events.put(('input', request('input-status')))
            except (OSError, ValueError) as error:
                self.events.put(('error', str(error)))

    def tick(self):
        while not self.events.empty():
            kind, value = self.events.get_nowait()
            if kind == 'state':
                self.data, self.error = value, None
            elif kind == 'error':
                self.error = value
            elif kind == 'input':
                self.input_state = value
            elif kind == 'callback':
                callback, result, error = value
                try:
                    callback(result, error)
                except tk.TclError:
                    pass
            else:
                self.notice = value
        self.draw()
        self.update_panel()
        # Flush Tk's deferred geometry/painting before publishing UI readiness.
        # A live polling loop alone does not prove that the first frame reached X.
        self.root.update_idletasks()
        if self.data and self.data.get('platform') == 'hyperv-dev' and not self.virtual_pointer_initialized:
            # Hyper-V's basic console can retain a black first frame until its
            # virtual pointer is initialized. Place it once in the empty header,
            # without clicking or requiring the owner to move the mouse.
            self.canvas.event_generate('<Motion>', x=48, y=48, warp=True)
            self.virtual_pointer_initialized = True
            self.root.update_idletasks()
        if self.data and os.environ.get('DISPLAY') == ':0':
            health = Path.home()/'.local/state/companion/session-health.json'
            try:
                atomic_write(health, json.dumps({'pid': os.getpid(), 'uid': os.getuid(),
                    'release': str(Path(__file__).resolve().parent), 'version': '0.4',
                    'boot_id': self.data['boot_id'], 'page': self.page, 'apps': self.apps.running()}))
            except OSError:
                self.notice = 'Session state could not be saved; check available disk space'
        self.root.after(1000, self.tick)

    def show(self, page):
        self.page = page
        self.focus_index = -1
        self.draw()
        if page == 'workspace':
            self.root.focus_force()

    @staticmethod
    def panel_button(label, callback, parent, accent=False):
        button = tk.Button(parent, text=label, command=callback, relief='flat', bd=0,
                           bg='#50e1be' if accent else '#213447',
                           fg='#09251f' if accent else '#f0f5ff',
                           activebackground='#78ecd2', activeforeground='#09251f',
                           font=('DejaVu Sans', 11, 'bold' if accent else 'normal'),
                           padx=12, pady=9, takefocus=True, highlightthickness=0)
        button.pack(side='left', padx=3, pady=10)
        return button

    def open_menu(self):
        try:
            self.panel_menu.tk_popup(self.panel.winfo_rootx()+15,
                                     self.panel.winfo_rooty()-self.panel_menu.yposition('end')-16)
        finally:
            self.panel_menu.grab_release()

    def reserve_panel_space(self):
        # Keep maximized windows above the dock while leaving the desktop itself full size.
        try:
            windows = subprocess.check_output(['xdotool', 'search', '--name', '^Companion panel$'],
                                              text=True, timeout=2, stderr=subprocess.DEVNULL).split()
            if len(windows) != 1:
                return
            subprocess.run(['xprop', '-id', windows[0], '-f', '_NET_WM_STRUT',
                            '32c', '-set', '_NET_WM_STRUT', f'0, 0, 0, {self.panel_height+16}'],
                           check=True, timeout=2, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
        except (OSError, subprocess.SubprocessError):
            pass

    def update_panel(self):
        self.panel_clock.configure(text=time.strftime('%H:%M'))
        if self.data:
            network = self.data.get('network') or []
            self.panel_status.configure(text=(network[0]['address'] if network else 'Offline'))
        try:
            listing = subprocess.check_output(['xprop', '-root', '_NET_CLIENT_LIST'], text=True,
                                              timeout=2, stderr=subprocess.DEVNULL)
            windows = []
            for window in re.findall(r'0x[0-9a-fA-F]+', listing):
                kind = subprocess.check_output(['xprop', '-id', window, '_NET_WM_WINDOW_TYPE'],
                                               text=True, timeout=2, stderr=subprocess.DEVNULL)
                if any(value in kind for value in ('_DESKTOP', '_DOCK')):
                    continue
                title = subprocess.check_output(['xdotool', 'getwindowname', window],
                                                text=True, timeout=2, stderr=subprocess.DEVNULL).strip()
                if title and title != 'Companion — Switch applications':
                    windows.append((window, title))
            signature = tuple(windows)
        except (OSError, subprocess.SubprocessError):
            return
        if signature != self.task_signature:
            for child in self.panel_tasks.winfo_children():
                child.destroy()
            for window, title in windows[:3]:
                short = title.removeprefix('Companion — ')
                self.panel_button(short[:16],
                                  lambda target=window: subprocess.Popen(['xdotool', 'windowactivate', target]),
                                  self.panel_tasks)
            if len(windows) > 3:
                self.panel_button(f'+{len(windows)-3}', self.apps.switcher, self.panel_tasks)
            self.task_signature = signature

    def focus_next(self, event=None):
        if self.buttons:
            self.focus_index = (self.focus_index + 1) % len(self.buttons)
            self.draw()
        return 'break'

    def activate(self, event=None):
        if 0 <= self.focus_index < len(self.buttons):
            self.buttons[self.focus_index][1]()
        return 'break'

    def location(self, event):
        return ((event.x-self.layout[0])/self.layout[2], (event.y-self.layout[1])/self.layout[2])

    def hit(self, point):
        x, y = point
        for index, (rect, action) in enumerate(self.buttons):
            x1, y1, x2, y2 = rect
            if x1 <= x <= x2 and y1 <= y <= y2:
                return index
        return None

    def press(self, event):
        self.pointer_start = self.location(event)
        self.pressed = self.hit(self.pointer_start)
        if self.pressed is not None:
            self.focus_index = self.pressed
        self.pointer_count += 1
        if self.page == 'input' and self.in_pad(self.pointer_start):
            self.ink.append(None)
            self.ink.append(self.pointer_start)
        self.draw()

    def release(self, event):
        point = self.location(event)
        index = self.hit(point)
        start = self.pointer_start
        pressed = self.pressed
        self.pressed = None
        self.pointer_start = None
        # A drag outside a button must not trigger a setting change.
        if start and pressed is not None and pressed == index and abs(point[0]-start[0]) < 15 and abs(point[1]-start[1]) < 15:
            self.buttons[index][1]()
        self.draw()

    @staticmethod
    def in_pad(point):
        return 610 <= point[0] <= 1207 and 292 <= point[1] <= 477

    def drag(self, event):
        point = self.location(event)
        if self.page == 'input' and self.in_pad(point):
            self.ink.append(point)
            self.ink = self.ink[-600:]
            self.draw()
        elif self.page == 'input' and self.pointer_start and 610 <= self.pointer_start[0] <= 1207 and 520 <= self.pointer_start[1] <= 630:
            delta = point[1] - self.pointer_start[1]
            if abs(delta) >= 24:
                self.scroll(-1 if delta > 0 else 1)
                self.pointer_start = point

    def hover(self, event):
        self.canvas.configure(cursor='hand2' if self.hit(self.location(event)) is not None else 'arrow')

    def scroll(self, delta):
        if self.page == 'input':
            count = len(self.data['thermal']) if self.data else 0
            self.scroll_position = max(0, min(max(0, count-3), self.scroll_position + delta))
            self.draw()

    def input_setting(self, key, value):
        self.commands.put(('input-settings', {key: value}))

    def clear_pad(self):
        self.ink = []
        self.pointer_count = 0
        self.draw()

    def launch(self, app):
        try:
            self.apps.launch(app)
        except (OSError, ValueError) as error:
            messagebox.showerror('Application unavailable', str(error), parent=self.root)

    def administrative_console(self):
        if messagebox.askyesno('Administrator console', 'Open a local terminal with root control of this computer?', parent=self.root):
            self.apps.call('admin-console', 'admin', self.action_result)

    def action_result(self, result, error):
        if error:
            messagebox.showerror('System action failed', error, parent=self.root)
        else:
            self.notice = 'System action completed'

    def power(self, operation):
        label = 'Restart' if operation == 'reboot' else 'Shut down'
        if messagebox.askyesno(label, label+' this computer? Save documents first.', parent=self.root):
            self.apps.call('power', {'operation': operation, 'confirm': True}, self.action_result)

    def draw(self):
        c = self.canvas
        width, height = max(800, c.winfo_width()), max(600, c.winfo_height())
        scale = min(width / 1280, height / 800)
        offset_x = (width - 1280 * scale) / 2
        offset_y = (height - 800 * scale) / 2
        self.layout = (offset_x, offset_y, scale)
        light = self.data and self.data['preferences']['theme'] == 'light'
        bg, panel, fg, muted, accent = (('#eff3f7', '#ffffff', '#142135', '#526278', '#087f74') if light
                                       else ('#0b1018', '#131d2a', '#f0f5ff', '#90a1b8', '#50e1be'))
        c.configure(bg=bg)
        c.delete('all')
        self.buttons = []
        def text(x, y, value, size=15, color=fg, weight='normal', anchor='nw'):
            return c.create_text(offset_x + x * scale, offset_y + y * scale, text=value, anchor=anchor, fill=color,
                                 font=('DejaVu Sans', max(10, round(size * scale)), weight))
        def box(x, y, w, h, color=panel):
            return c.create_rectangle(offset_x+x * scale, offset_y+y * scale,
                                      offset_x+(x+w) * scale, offset_y+(y+h) * scale,
                                      fill=color, outline=color)
        def button(x, y, label, callback, w=160):
            index = len(self.buttons)
            self.buttons.append(((x, y, x+w, y+48), callback))
            item = box(x, y, w, 48, accent)
            if self.focus_index == index:
                c.itemconfigure(item, outline=fg, width=3)
            text(x+w/2, y+24, label, 13, '#09251f' if not light else '#ffffff', 'bold', 'center')
        d = self.data
        if self.page == 'workspace':
            self.draw_workspace(text, box, button, d, accent, muted)
            return
        text(48, 38, 'C O M P A N I O N', 23, accent, 'bold')
        text(1232, 43, time.strftime('%H:%M'), 18, muted, anchor='ne')
        title = {'workspace': 'Your workspace', 'overview': 'System overview', 'input': 'Input and interaction',
                 'device': 'Device and management'}[self.page]
        text(48, 102, title, 36, weight='bold')
        subtitle = {'workspace': 'Applications and settings', 'overview': 'Live system state',
                    'input': 'Input settings', 'device': 'Hardware and management'}[self.page]
        text(830, 193, subtitle, 12, muted)
        if not d:
            text(48, 240, 'Starting Companion…', 24)
            text(48, 294, self.error or self.notice, 14, muted)
            return
        for x, label, page in ((48, 'Home', 'workspace'), (236, 'Overview', 'overview'), (424, 'Input', 'input'), (612, 'Device', 'device')):
            button(x, 176, label, lambda p=page: self.show(p), 160)
            if self.page == page:
                box(x, 227, 160, 3, accent)
        if self.page == 'input':
            box(48, 240, 510, 408)
            box(585, 240, 647, 408)
            text(72, 260, 'TOUCHPAD', 12, accent, 'bold')
            settings = d['preferences'].get('input', {'tap': True, 'natural_scroll': True, 'speed': 0.0})
            state = self.input_state or settings
            if d.get('platform') == 'hyperv-dev' and self.input_state is None:
                text(72, 310, 'No physical touchpad in this VM', 18)
                text(72, 370, 'Virtual keyboard and pointer active', 15, muted)
                text(72, 440, 'Use the drawing area to test interaction', 15, muted)
            else:
                button(72, 301, 'Tap-to-click: ' + ('On' if state['tap'] else 'Off'),
                       lambda: self.input_setting('tap', not state['tap']), 300)
                button(72, 370, 'Natural scroll: ' + ('On' if state['natural_scroll'] else 'Off'),
                       lambda: self.input_setting('natural_scroll', not state['natural_scroll']), 300)
                text(72, 442, 'Pointer speed', 17)
                speed = state['speed']
                text(310, 442, f'{speed:+.1f}', 17, accent)
                button(72, 483, 'Slower', lambda: self.input_setting('speed', max(-1.0, round(speed-0.2, 1))), 150)
                button(242, 483, 'Faster', lambda: self.input_setting('speed', min(1.0, round(speed+0.2, 1))), 150)
                text(72, 556, 'Two-finger scrolling  ·  Tap and drag', 13, muted)
                text(72, 592, 'Typing rejection enabled' if state.get('typing_rejection') else 'Checking input driver…', 13, muted)
            text(610, 260, 'TOUCHSCREEN / DRAG TEST', 12, accent, 'bold')
            box(610, 292, 597, 185, bg)
            text(632, 309, 'Draw here with a finger or pointer', 15, muted)
            prior = None
            for point in self.ink:
                if point and prior:
                    c.create_line(offset_x+prior[0]*scale, offset_y+prior[1]*scale,
                                  offset_x+point[0]*scale, offset_y+point[1]*scale, fill=accent, width=3*scale)
                prior = point
            text(610, 494, 'Two-finger scroll test: sensor list', 13, muted)
            for row, sensor in enumerate(d['thermal'][self.scroll_position:self.scroll_position+3]):
                text(632, 529+row*28, f"{sensor['name']}     {sensor['celsius']:.0f}°C", 13)
            button(1057, 580, 'Clear', self.clear_pad, 150)
            text(610, 615, f'{self.pointer_count} pointer presses', 11, muted)
        elif self.page == 'device':
            box(48, 240, 1184, 408)
            text(72, 264, 'HARDWARE & MANAGEMENT', 12, accent, 'bold')
            for row, (label, value) in enumerate((('Device', d['bios'].get('model')),
                                                 ('Firmware', d['bios'].get('version')),
                                                 ('Linux kernel', d['kernel']), ('Host', d['hostname']),
                                                 ('Boot identity', d['boot_id']))):
                text(72, 314+row*48, label, 15, muted)
                text(310, 314+row*48, value or 'Unavailable', 15)
            text(72, 590, 'Root management and recovery run independently of this interface.', 15, accent)
        else:
            self.draw_overview(text, box, button, d, accent, muted)
        text(48, 682, d['bios'].get('model') or d['hostname'], 14, muted)
        if self.error:
            text(500, 682, 'Control unavailable — '+self.error, 11, '#e5a457')

    def draw_workspace(self, text, box, button, d, accent, muted):
        c = self.canvas
        ox, oy, scale = self.layout
        box(0, 0, 1280, 800, '#0a1420')
        for x, y, radius, color in ((960, 560, 490, '#102b3c'),
                                    (1040, 590, 340, '#123c4c'),
                                    (1110, 640, 210, '#175265')):
            c.create_oval(ox+(x-radius)*scale, oy+(y-radius)*scale,
                          ox+(x+radius)*scale, oy+(y+radius)*scale,
                          fill=color, outline=color)
        text(56, 42, 'C O M P A N I O N', 18, accent, 'bold')
        text(56, 107, 'Your desktop', 38, weight='bold')
        text(58, 169, 'Open a file or application to begin.', 14, muted)
        text(1220, 62, time.strftime('%A, %d %B'), 15, muted, anchor='ne')
        text(1220, 100, time.strftime('%H:%M'), 46, anchor='ne')
        if not d:
            text(58, 235, self.error or 'Connecting to the local control service…', 14, muted)
            return
        virtual = d.get('platform') == 'hyperv-dev'
        addresses = d.get('network') or []
        text(1220, 175, 'Ethernet '+addresses[0]['address'] if addresses else 'Offline',
             13, accent if addresses else muted, anchor='ne')
        text(1220, 207, 'Virtual audio · No speakers' if virtual else d['bios'].get('model') or d['hostname'],
             12, muted, anchor='ne')
        def icon(x, y, mark, label, callback):
            tile = box(x, y, 132, 106, '#17283a')
            index = len(self.buttons)
            self.buttons.append(((x, y, x+132, y+106), callback))
            if self.focus_index == index:
                c.itemconfigure(tile, outline=accent, width=3)
            box(x+14, y+12, 43, 43, '#286c76')
            text(x+35, y+33, mark, 23, '#f0f5ff', 'bold', 'center')
            text(x+14, y+73, label, 15, '#f0f5ff')
        icon(58, 262, 'F', 'Files', lambda: self.launch('files'))
        icon(208, 262, 'E', 'Editor', lambda: self.launch('editor'))
        icon(58, 386, 'B', 'Browser', lambda: self.launch('browser'))
        icon(208, 386, '>', 'Terminal', lambda: self.launch('terminal'))
        icon(58, 510, 'K', 'Keyboard', lambda: self.launch('keyboard'))
        icon(208, 510, 'S', 'Settings', lambda: self.show('overview'))
        icon(358, 510, 'N', 'Network', lambda: self.launch('network'))
        text(58, 674, 'Companion on Linux  ·  Windows+D: desktop  ·  Alt+Tab: switch windows', 12, muted)

    def draw_overview(self, text, box, button, d, accent, muted):
        light = d['preferences']['theme'] == 'light'
        cards = [(48, 'CONNECTION'), (451, 'POWER'), (854, 'SYSTEM')]
        for x, title in cards:
            box(x, 240, 378, 226)
            text(x+24, 251, title, 12, accent, 'bold')
        addresses = d['network']
        text(72, 298, addresses[0]['address'] if addresses else 'Offline', 27, weight='bold')
        text(72, 352, ', '.join(a['interface'] for a in addresses) or 'No IPv4 address', 14, muted)
        mg = d['management']
        text(72, 402, 'SSH + recovery watch ready' if mg['ssh'] and mg['watch'] else 'Management needs attention',
             13, accent if mg['ssh'] and mg['watch'] else '#e5a457')
        bat = d['batteries'][0] if d['batteries'] else None
        text(475, 298, f"{bat['capacity']}%" if bat and bat['capacity'] is not None else '—', 34, weight='bold')
        text(475, 358, bat['status'] if bat else 'Battery unavailable', 15, muted)
        if any(p['online'] == 1 for p in d['power']):
            power_state = 'External power connected'
        elif bat:
            power_state = 'Running on battery'
        else:
            power_state = 'Power source unavailable'
        text(475, 406, power_state, 13, muted)
        mem = d['memory']
        total, avail = mem.get('MemTotal', 0), mem.get('MemAvailable', 0)
        text(878, 298, f'{(total-avail)/2**30:.1f} / {total/2**30:.1f} GB', 25, weight='bold')
        temps = d['thermal']
        text(878, 358, f"{max(t['celsius'] for t in temps):.0f}°C highest sensor" if temps else 'Temperature unavailable', 14, muted)
        text(878, 406, f"Up {int(d['uptime_seconds']//3600)}h {int(d['uptime_seconds']//60)%60}m", 14, muted)
        box(48, 491, 1184, 157)
        text(72, 514, 'DISPLAY & APPEARANCE', 12, accent, 'bold')
        brightness = d['brightness']
        text(72, 561, f'Brightness   {brightness}%' if brightness is not None else 'Backlight unavailable', 18)
        if brightness is not None:
            button(381, 548, '−', lambda: self.commands.put(('brightness', max(5, brightness-10))), 64)
            button(457, 548, '+', lambda: self.commands.put(('brightness', min(100, brightness+10))), 64)
        button(690, 548, 'Night / light', lambda: self.commands.put(('theme', 'night' if light else 'light')), 190)
        button(909, 548, 'Refresh', lambda: self.commands.put(('status', None)), 160)

    def run(self):
        self.root.mainloop()


if __name__ == '__main__':
    Shell().run()
