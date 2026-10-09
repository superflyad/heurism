"""Companion applications run with the session user's normal Linux permissions."""
import json
import os
from pathlib import Path
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk
import time
import stat
import secrets

HOME = Path.home()
DOCUMENTS = HOME / 'Documents'
STATE = HOME / '.local/state/companion'


def atomic_write(path, text):
    path = Path(path).resolve()
    mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o600
    temporary = path.parent / ('.' + path.name + '.companion-' + secrets.token_hex(6))
    try:
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
        with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


class Window:
    def __init__(self, manager, title):
        self.manager = manager
        self.win = tk.Toplevel(manager.shell.root)
        self.win.title('Companion — ' + title)
        screen_width = manager.shell.root.winfo_screenwidth()
        screen_height = manager.shell.root.winfo_screenheight()
        width = min(1100, screen_width-80)
        height = min(700, screen_height-150)
        x = (screen_width-width)//2
        y = max(35, (screen_height-84-height)//2)
        self.win.geometry(f'{width}x{height}+{x}+{y}')
        self.win.minsize(min(720, width), min(480, height))
        self.win.configure(bg='#131d2a')
        self.win.protocol('WM_DELETE_WINDOW', self.close)
        bar = tk.Frame(self.win, bg='#131d2a')
        bar.pack(fill='x', padx=18, pady=12)
        tk.Label(bar, text=title, bg='#131d2a', fg='#f0f5ff', font=('DejaVu Sans', 20, 'bold')).pack(side='left')
        self.button(bar, 'Close', self.close).pack(side='right', padx=4)
        self.button(bar, 'Keyboard', lambda: manager.launch('keyboard')).pack(side='right', padx=4)
        self.button(bar, 'Home', manager.home).pack(side='right', padx=4)
        self.body = tk.Frame(self.win, bg='#131d2a')
        self.body.pack(fill='both', expand=True, padx=18, pady=(0, 18))
        self.win.after(100, self.win.focus_force)

    @staticmethod
    def button(parent, label, action):
        return tk.Button(parent, text=label, command=action, font=('DejaVu Sans', 13),
                         bg='#50e1be', fg='#09251f', activebackground='#87ead3',
                         padx=12, pady=10, relief='flat', takefocus=True)

    def close(self):
        self.win.destroy()


class Editor(Window):
    def __init__(self, manager):
        super().__init__(manager, 'Editor')
        self.path = None
        self.dirty = False
        self.loading = False
        self.draft = STATE / 'editor-draft.json'
        toolbar = tk.Frame(self.body, bg='#131d2a')
        toolbar.pack(fill='x', pady=(0, 10))
        for label, action in [('New', self.new), ('Open', self.open), ('Save', self.save), ('Save as', self.save_as)]:
            self.button(toolbar, label, action).pack(side='left', padx=(0, 8))
        self.name = tk.StringVar(value='Untitled')
        tk.Label(self.body, textvariable=self.name, bg='#131d2a', fg='#90a1b8', anchor='w',
                 font=('DejaVu Sans', 12)).pack(fill='x', pady=(0, 8))
        area = tk.Frame(self.body)
        area.pack(fill='both', expand=True)
        self.text = tk.Text(area, undo=True, wrap='word', font=('DejaVu Sans Mono', 14),
                            bg='#0b1018', fg='#f0f5ff', insertbackground='#50e1be', padx=14, pady=12)
        scroll = ttk.Scrollbar(area, command=self.text.yview)
        self.text.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y')
        self.text.pack(fill='both', expand=True)
        self.text.bind('<<Modified>>', self.changed)
        self.win.bind('<Control-s>', lambda e: self.save())
        self.win.bind('<Control-o>', lambda e: self.open())
        self.win.bind('<Control-n>', lambda e: self.new())
        self.win.after(1500, self.autosave)
        STATE.mkdir(parents=True, exist_ok=True)
        if self.draft.exists():
            try:
                draft = json.loads(self.draft.read_text())
                if messagebox.askyesno('Recover document', 'Restore the editor document from the previous session?', parent=self.win):
                    self.load_text(draft['text'], draft.get('path'))
                    self.dirty = True
            except (OSError, ValueError, KeyError):
                pass

    def changed(self, event=None):
        if self.text.edit_modified():
            if not self.loading:
                self.dirty = True
                self.name.set(str(self.path or 'Untitled') + ' • Unsaved')
            self.text.edit_modified(False)

    def load_text(self, text, path=None):
        self.loading = True
        self.text.delete('1.0', 'end')
        self.text.insert('1.0', text)
        self.text.edit_modified(False)
        self.text.edit_reset()
        self.loading = False
        self.path = Path(path) if path else None
        self.dirty = False
        self.name.set(str(self.path or 'Untitled'))

    def may_discard(self):
        if not self.dirty:
            return True
        answer = messagebox.askyesnocancel('Unsaved document', 'Save this document before continuing?', parent=self.win)
        if answer is None:
            return False
        return self.save() if answer else True

    def new(self):
        if self.may_discard():
            self.load_text('')
            self.draft.unlink(missing_ok=True)

    def open(self, path=None):
        if not self.may_discard():
            return
        path = path or filedialog.askopenfilename(parent=self.win, initialdir=DOCUMENTS)
        if not path:
            return
        try:
            if Path(path).stat().st_size > 4*1024*1024:
                raise ValueError('This editor opens text files up to 4 MiB.')
            text = Path(path).read_text(encoding='utf-8')
            if '\0' in text:
                raise ValueError('This is a binary file, not a text document.')
            self.load_text(text, path)
            self.draft.unlink(missing_ok=True)
        except (OSError, ValueError) as error:
            messagebox.showerror('Open failed', str(error), parent=self.win)

    def save_as(self):
        path = filedialog.asksaveasfilename(parent=self.win, initialdir=DOCUMENTS,
                                           initialfile=self.path.name if self.path else 'Untitled.txt',
                                           defaultextension='.txt', confirmoverwrite=True)
        return self.save(path) if path else False

    def save(self, path=None):
        destination = Path(path) if path else self.path
        if destination is None:
            return self.save_as()
        try:
            atomic_write(destination, self.text.get('1.0', 'end-1c'))
            self.path = destination
            self.dirty = False
            self.name.set(str(destination) + ' • Saved')
            self.draft.unlink(missing_ok=True)
            return True
        except OSError as error:
            messagebox.showerror('Save failed', str(error), parent=self.win)
            return False

    def autosave(self):
        if not self.win.winfo_exists():
            return
        if self.dirty:
            try:
                atomic_write(self.draft, json.dumps({'path': str(self.path) if self.path else None,
                                                     'text': self.text.get('1.0', 'end-1c')}))
            except OSError:
                self.name.set('Draft could not be saved — use Save')
        self.win.after(1500, self.autosave)

    def close(self):
        if self.may_discard():
            self.draft.unlink(missing_ok=True)
            super().close()


class Files(Window):
    def __init__(self, manager):
        super().__init__(manager, 'Files')
        self.path = DOCUMENTS
        self.entries = {}
        bar = tk.Frame(self.body, bg='#131d2a')
        bar.pack(fill='x', pady=(0, 10))
        for label, action in [('Home', lambda: self.navigate(HOME)), ('Up', lambda: self.navigate(self.path.parent)),
                              ('Open', self.open_selected), ('New folder', self.mkdir), ('Rename', self.rename),
                              ('Trash', self.trash), ('Refresh', self.refresh)]:
            self.button(bar, label, action).pack(side='left', padx=(0, 6))
        trashbar = tk.Frame(self.body, bg='#131d2a')
        trashbar.pack(fill='x', pady=(0, 10))
        self.button(trashbar, 'Trash bin', self.show_trash).pack(side='left', padx=(0, 8))
        self.button(trashbar, 'Restore selected', self.restore).pack(side='left')
        self.location = tk.StringVar()
        address = tk.Entry(self.body, textvariable=self.location, font=('DejaVu Sans', 14))
        address.pack(fill='x', ipady=8, pady=(0, 10))
        address.bind('<Return>', lambda e: self.navigate(Path(self.location.get()).expanduser()))
        style = ttk.Style(self.win)
        style.configure('Companion.Treeview', font=('DejaVu Sans', 14), rowheight=44,
                        background='#0b1018', fieldbackground='#0b1018', foreground='#f0f5ff')
        style.configure('Companion.Treeview.Heading', font=('DejaVu Sans', 13))
        area = tk.Frame(self.body)
        area.pack(fill='both', expand=True)
        self.tree = ttk.Treeview(area, columns=('type', 'size'), style='Companion.Treeview', selectmode='browse')
        self.tree.heading('#0', text='Name')
        self.tree.heading('type', text='Kind')
        self.tree.heading('size', text='Size')
        self.tree.column('#0', width=570)
        self.tree.column('type', width=150)
        self.tree.column('size', width=120)
        scroll = ttk.Scrollbar(area, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y')
        self.tree.pack(fill='both', expand=True)
        self.tree.bind('<Double-1>', lambda e: self.open_selected())
        self.tree.bind('<Return>', lambda e: self.open_selected())
        self.tree.bind('<ButtonPress-1>', lambda e: setattr(self, 'drag_y', e.y), add='+')
        self.tree.bind('<B1-Motion>', self.drag_scroll)
        self.navigate(self.path)

    def drag_scroll(self, event):
        delta = event.y-getattr(self, 'drag_y', event.y)
        if abs(delta) >= 20:
            self.tree.yview_scroll(-1 if delta > 0 else 1, 'units')
            self.drag_y = event.y
            return 'break'

    def navigate(self, path):
        try:
            path = Path(path).resolve(strict=True)
            if not path.is_dir():
                raise ValueError('Select a directory')
            entries = sorted(path.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower()))
            if path == HOME / '.local/share/companion/trash':
                entries = [p for p in entries if not p.name.endswith('.origin.json')]
            if len(entries) > 2000:
                raise ValueError('This folder contains more than 2000 entries; use Terminal.')
            self.path = path
            self.location.set(str(path))
            self.tree.delete(*self.tree.get_children())
            self.entries = {}
            for index, item in enumerate(entries):
                try:
                    size = item.stat().st_size
                except OSError:
                    size = 0
                key = str(index)
                self.entries[key] = item
                self.tree.insert('', 'end', iid=key, text=item.name,
                                 values=('Folder' if item.is_dir() else 'File', '' if item.is_dir() else f'{size:,} B'))
        except (OSError, ValueError) as error:
            messagebox.showerror('Folder unavailable', str(error), parent=self.win)

    def refresh(self):
        self.navigate(self.path)

    def selected(self):
        rows = self.tree.selection()
        return self.entries.get(rows[0]) if rows else None

    def open_selected(self):
        item = self.selected()
        if item:
            if item.is_dir():
                self.navigate(item)
            else:
                self.manager.editor(item)

    def basename(self, prompt, initial=''):
        name = simpledialog.askstring('Files', prompt, initialvalue=initial, parent=self.win)
        if name is None:
            return None
        if not name or name in ('.', '..') or '/' in name or '\0' in name:
            messagebox.showerror('Invalid name', 'Use a single nonempty filename.', parent=self.win)
            return None
        return name

    def mkdir(self):
        name = self.basename('Folder name')
        if name:
            try:
                (self.path/name).mkdir()
                self.refresh()
            except OSError as error:
                messagebox.showerror('Create failed', str(error), parent=self.win)

    def rename(self):
        item = self.selected()
        if item:
            name = self.basename('New name', item.name)
            if name:
                try:
                    destination = item.with_name(name)
                    if destination.exists():
                        raise ValueError('That name already exists.')
                    item.rename(destination)
                    self.refresh()
                except (OSError, ValueError) as error:
                    messagebox.showerror('Rename failed', str(error), parent=self.win)

    def trash(self):
        item = self.selected()
        if self.path == HOME / '.local/share/companion/trash':
            return
        if item and messagebox.askyesno('Move to Trash', f'Move {item.name} to Trash?', parent=self.win):
            try:
                directory = HOME / '.local/share/companion/trash'
                directory.mkdir(parents=True, exist_ok=True)
                destination = directory / (str(time.time_ns())+'-'+item.name)
                item.rename(destination)
                atomic_write(destination.with_name(destination.name+'.origin.json'), json.dumps({'original': str(item)}))
                self.refresh()
            except OSError as error:
                messagebox.showerror('Trash failed', str(error), parent=self.win)

    def show_trash(self):
        directory = HOME / '.local/share/companion/trash'
        directory.mkdir(parents=True, exist_ok=True)
        self.navigate(directory)

    def restore(self):
        item = self.selected()
        if not item or self.path != HOME / '.local/share/companion/trash':
            return
        metadata = item.with_name(item.name+'.origin.json')
        try:
            destination = Path(json.loads(metadata.read_text())['original'])
            if not destination.is_absolute() or destination.exists() or not destination.parent.is_dir():
                raise ValueError('The original location is unavailable or already occupied.')
            item.rename(destination)
            metadata.unlink()
            self.refresh()
        except (OSError, ValueError, KeyError) as error:
            messagebox.showerror('Restore failed', str(error), parent=self.win)


class Applications:
    def __init__(self, shell):
        self.shell = shell
        self.windows = {}
        self.processes = {}
        DOCUMENTS.mkdir(parents=True, exist_ok=True)
        STATE.mkdir(parents=True, exist_ok=True)
        style = ttk.Style(shell.root)
        style.configure('Companion.Treeview', font=('DejaVu Sans', 14), rowheight=44,
                        background='#0b1018', fieldbackground='#0b1018', foreground='#f0f5ff')
        style.configure('Companion.Treeview.Heading', font=('DejaVu Sans', 13))

    def home(self):
        import home
        home.main()

    def internal(self, name, cls):
        window = self.windows.get(name)
        if window is None or not window.win.winfo_exists():
            window = cls(self)
            self.windows[name] = window
        window.win.deiconify()
        window.win.lift()
        window.win.focus_force()
        return window

    def editor(self, path=None):
        editor = self.internal('editor', Editor)
        if path:
            editor.open(path)
        return editor

    def launch(self, name):
        if name == 'files':
            return self.internal(name, Files)
        if name == 'editor':
            return self.editor()
        for app, cls in (('network', Network), ('firmware', Firmware), ('sound', Sound)):
            if name == app:
                return self.internal(name, cls)
        commands = {'terminal': ['xterm', '-T', 'Companion Terminal', '-fa', 'DejaVu Sans Mono', '-fs', '14'],
                    'browser': ['firefox', '--new-window', 'file:///opt/companion/desktop/start.html'],
                    'keyboard': ['onboard', '--theme', 'Nightshade', '--size', f'{min(1200, self.shell.root.winfo_screenwidth())}x320',
                                 '-x', str(max(0, (self.shell.root.winfo_screenwidth()-1200)//2)),
                                 '-y', str(max(0, self.shell.root.winfo_screenheight()-self.shell.panel_height-346))]}
        if name not in commands:
            raise ValueError('Unknown application')
        process = self.processes.get(name)
        if process and process.poll() is None:
            if name == 'keyboard':
                subprocess.run(['dbus-send', '--session', '--type=method_call', '--dest=org.onboard.Onboard',
                                '/org/onboard/Onboard/Keyboard', 'org.onboard.Onboard.Keyboard.Show'], timeout=2)
                return process
            result = subprocess.run(['xdotool', 'search', '--pid', str(process.pid)],
                                    capture_output=True, text=True, timeout=2)
            if result.stdout.split():
                subprocess.Popen(['xdotool', 'windowactivate', result.stdout.split()[0]])
            return process
        log = (STATE/(name+'.log')).open('a')
        process = subprocess.Popen(commands[name], stdout=log, stderr=subprocess.STDOUT)
        log.close()
        self.processes[name] = process
        def check_launch():
            if process.poll() not in (None, 0):
                self.shell.notice = f'{name.capitalize()} could not start; see {STATE/(name+".log")}'
        self.shell.root.after(2000, check_launch)
        return process

    def switcher(self):
        switcher = self.internal('windows', Switcher)
        switcher.refresh()
        return switcher

    def running(self):
        return ([name for name, window in self.windows.items() if window.win.winfo_exists()] +
                [name for name, process in self.processes.items() if process.poll() is None])

    def call(self, action, value, callback):
        import threading
        from shell import request
        def job():
            try:
                result, error = request(action, value), None
            except (OSError, ValueError) as problem:
                result, error = None, str(problem)
            self.shell.events.put(('callback', (callback, result, error)))
        threading.Thread(target=job, daemon=True).start()


class Switcher(Window):
    def __init__(self, manager):
        super().__init__(manager, 'Switch applications')
        self.tree = ttk.Treeview(self.body, show='tree', style='Companion.Treeview', selectmode='browse')
        self.tree.pack(fill='both', expand=True)
        self.tree.bind('<Double-1>', lambda e: self.activate())
        self.tree.bind('<Return>', lambda e: self.activate())
        self.button(self.body, 'Open selected application', self.activate).pack(anchor='w', pady=12)
        self.refresh()

    def refresh(self):
        import re
        try:
            listing = subprocess.check_output(['xprop', '-root', '_NET_CLIENT_LIST'], text=True, timeout=2)
            self.tree.delete(*self.tree.get_children())
            for window in re.findall(r'0x[0-9a-fA-F]+', listing):
                kind = subprocess.check_output(['xprop', '-id', window, '_NET_WM_WINDOW_TYPE'], text=True, timeout=2)
                name = subprocess.check_output(['xdotool', 'getwindowname', window], text=True, timeout=2).strip()
                if (not any(value in kind for value in ('_NET_WM_WINDOW_TYPE_DESKTOP',
                                                       '_NET_WM_WINDOW_TYPE_DOCK'))
                    and name != 'Companion — Switch applications'):
                    self.tree.insert('', 'end', iid=window, text=name or 'Application')
        except (OSError, subprocess.SubprocessError):
            pass

    def activate(self):
        rows = self.tree.selection()
        if rows:
            subprocess.Popen(['xdotool', 'windowactivate', rows[0]])
            self.close()


class Network(Window):
    def __init__(self, manager):
        super().__init__(manager, 'Network')
        self.wifi_available = manager.shell.data.get('platform') != 'hyperv-dev'
        self.message = tk.StringVar(value='Ethernet management stays connected. Scanning Wi-Fi…')
        tk.Label(self.body, textvariable=self.message, font=('DejaVu Sans', 13), fg='#90a1b8',
                 bg='#131d2a', wraplength=950, justify='left').pack(fill='x', pady=18)
        if not self.wifi_available:
            addresses = manager.shell.data.get('network') or []
            connection = ', '.join(f"{item['interface']} {item['address']}" for item in addresses)
            self.message.set((connection or 'No active IPv4 connection') +
                             '. This VM has no Wi-Fi adapter; Ethernet management remains independent of the desktop.')
            return
        tk.Label(self.body, text='Wi-Fi network', font=('DejaVu Sans', 16), bg='#131d2a', fg='white').pack(anchor='w')
        self.ssid = tk.StringVar()
        self.networks = ttk.Combobox(self.body, textvariable=self.ssid, font=('DejaVu Sans', 16))
        self.networks.pack(fill='x', ipady=10, pady=12)
        tk.Label(self.body, text='Password', font=('DejaVu Sans', 16), bg='#131d2a', fg='white').pack(anchor='w')
        self.password = tk.Entry(self.body, show='•', font=('DejaVu Sans', 16))
        self.password.pack(fill='x', ipady=10, pady=12)
        row = tk.Frame(self.body, bg='#131d2a')
        row.pack(fill='x', pady=12)
        self.button(row, 'Scan', self.scan).pack(side='left', padx=4)
        self.button(row, 'Connect', self.connect).pack(side='left', padx=4)
        self.scan()

    def scan(self):
        self.message.set('Scanning Wi-Fi…')
        self.manager.call('network-scan', None, self.scanned)

    def scanned(self, result, error):
        if not self.win.winfo_exists():
            return
        if error:
            self.message.set(error)
        else:
            self.networks.configure(values=result['networks'])
            self.message.set(f"{len(result['networks'])} networks on {result['interface']}. Ethernet remains available.")

    def connect(self):
        values = {'ssid': self.ssid.get(), 'password': self.password.get()}
        self.password.delete(0, 'end')
        self.message.set('Configuring Wi-Fi…')
        self.manager.call('network-connect', values, self.connected)

    def connected(self, result, error):
        if self.win.winfo_exists():
            self.message.set(error or result['state'])


class Firmware(Window):
    def __init__(self, manager):
        super().__init__(manager, 'BIOS settings')
        self.attributes = {}
        self.tree = ttk.Treeview(self.body, columns=('value',), selectmode='browse', style='Companion.Treeview')
        self.tree.heading('#0', text='Firmware attribute')
        self.tree.heading('value', text='Current value')
        self.tree.pack(fill='both', expand=True)
        self.tree.bind('<<TreeviewSelect>>', self.selected)
        self.choice = tk.StringVar()
        self.values = ttk.Combobox(self.body, textvariable=self.choice, state='readonly', font=('DejaVu Sans', 14))
        self.values.pack(fill='x', ipady=8, pady=10)
        self.apply_button = self.button(self.body, 'Apply selected value', self.apply)
        self.apply_button.pack(anchor='w')
        self.apply_button.configure(state='disabled')
        self.message = tk.StringVar(value='Reading supported BIOS attributes…')
        tk.Label(self.body, textvariable=self.message, font=('DejaVu Sans', 12), bg='#131d2a', fg='#90a1b8',
                 wraplength=950).pack(fill='x', pady=12)
        manager.call('bios-list', None, self.loaded)

    def loaded(self, result, error):
        if not self.win.winfo_exists():
            return
        if error:
            self.message.set(error)
            return
        self.attributes = result
        self.tree.delete(*self.tree.get_children())
        for name, item in sorted(result.items()):
            self.tree.insert('', 'end', iid=name, text=item.get('display_name') or name,
                             values=(item.get('current_value'),))
        self.message.set('Keyboard presentation settings can be changed. Boot/security attributes are view-only here.')

    def selected(self, event=None):
        rows = self.tree.selection()
        if rows:
            info = self.attributes[rows[0]]
            values = [v for v in (info.get('possible_values') or '').split(';') if v]
            self.values.configure(values=values)
            self.choice.set(info.get('current_value') or '')
            self.apply_button.configure(state='normal' if info['writable'] else 'disabled')

    def apply(self):
        rows = self.tree.selection()
        if rows and messagebox.askyesno('BIOS change', 'Apply this supported keyboard setting?', parent=self.win):
            self.manager.call('bios-set', {'name': rows[0], 'value': self.choice.get()}, self.applied)

    def applied(self, result, error):
        if self.win.winfo_exists():
            if error:
                self.message.set(error)
            else:
                self.manager.call('bios-list', None, self.loaded)


class Sound(Window):
    def __init__(self, manager):
        super().__init__(manager, 'Sound')
        self.message = tk.StringVar(value='Reading audio device…')
        tk.Label(self.body, textvariable=self.message, bg='#131d2a', fg='#90a1b8',
                 font=('DejaVu Sans', 14), wraplength=950).pack(fill='x', pady=20)
        self.volume = tk.Scale(self.body, from_=0, to=100, orient='horizontal', label='Output volume',
                               font=('DejaVu Sans', 16), bg='#131d2a', fg='white', length=700)
        self.volume.pack(fill='x', pady=20)
        self.button(self.body, 'Apply volume', self.apply).pack(anchor='w', pady=12)
        self.button(self.body, 'Mute / unmute', self.mute).pack(anchor='w', pady=12)
        try:
            result = subprocess.check_output(['pactl', 'get-sink-volume', '@DEFAULT_SINK@'], text=True, timeout=2)
            import re
            self.volume.set(int(re.search(r'(\d+)%', result)[1]))
            sink = subprocess.check_output(['pactl', 'get-default-sink'], text=True, timeout=2).strip()
            self.message.set('Virtual audio sink; no physical speaker output.' if
                             manager.shell.data.get('platform') == 'hyperv-dev' else sink)
        except (OSError, subprocess.SubprocessError, TypeError):
            self.message.set('Audio device unavailable; see session audio log.')

    def apply(self):
        try:
            subprocess.run(['pactl', 'set-sink-volume', '@DEFAULT_SINK@', str(self.volume.get())+'%'], check=True, timeout=2)
            self.message.set('Volume updated')
        except (OSError, subprocess.SubprocessError):
            self.message.set('Volume could not be changed')

    def mute(self):
        try:
            subprocess.run(['pactl', 'set-sink-mute', '@DEFAULT_SINK@', 'toggle'], check=True, timeout=2)
            self.message.set('Mute toggled')
        except (OSError, subprocess.SubprocessError):
            self.message.set('Mute could not be changed')
