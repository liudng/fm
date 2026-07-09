# fm — Linux Dual-Pane File Manager

A lightweight dual-pane file manager built with Qt6 Widgets, featuring multi-tab support, file operations, favorites management, session restore, and volume management.

## Features

- **Dual-pane layout**: Switchable horizontal/vertical orientation, independently adjustable split ratio, single-pane hide support
- **Multi-tab**: Each pane supports multiple tabs with independent navigation history, drag-to-reorder, clone, and close
- **File operations**: Copy, move, rename, delete, trash, properties — executed asynchronously with a progress dialog
- **Conflict resolution**: Overwrite / skip / rename / apply-to-all options when encountering name collisions during copy/move
- **Favorites**: Save and restore the current dual-pane layout and tab paths with one click
- **Session restore**: Automatically saves and restores the last window state, pane layout, tab paths, and sort settings
- **Volume management**: File menu lists mounted volumes (QStorageInfo) and external removable devices (UDisks2) with mount/unmount/eject support
- **Clipboard**: Cut/copy/paste, cross-pane cut/copy, copy path/filename
- **Configurable shortcuts**: All shortcuts are customizable via the settings dialog with instant effect
- **Themes & icons**: UI theme (QStyleFactory) and icon theme are configurable
- **Internationalization**: Chinese / English switching

## Dependencies

### Runtime

```bash
sudo apt install libqt6widgets6 libqt6concurrent6 libqt6dbus6 libqt6network6 \
    udisks2 gnome-icon-theme
```

### Development Tools

```bash
sudo apt install build-essential cmake qt6-l10n-tools
```

### Development Libraries

```bash
sudo apt install qt6-base-dev qt6-base-dev-tools
```

## Build

```bash
cd fm-qt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The built binary is located at `build/fm`.

## Installation

```bash
sudo cmake --install build
```

Installs to `/usr/local/bin/fm` by default.

## Running

```bash
./build/fm
```

Or after installation:

```bash
fm
```

The application automatically restores the last session on startup. If an instance is already running, a new process brings the existing window to the foreground and exits (single-instance mode).

## Configuration

The configuration file is located at `~/.config/fm/fm.conf` (INI format). All settings can be modified through the Settings dialog and take effect immediately.

## Default Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | Cut / Copy / Paste |
| `F5` / `F6` | Copy to opposite / Cut to opposite |
| `Ctrl+N` / `F7` | New file / New folder |
| `Ctrl+Shift+O` | Open with... |
| `Return` / `F2` | Open / Rename |
| `Delete` / `Shift+Delete` | Move to trash / Delete permanently |
| `Alt+←` / `Alt+→` / `Alt+↑` | Back / Forward / Up |
| `Ctrl+R` | Refresh |
| `Ctrl+T` / `Ctrl+W` / `Ctrl+Shift+T` | New tab / Close tab / Clone tab |
| `Ctrl+Tab` | Next tab |
| `Tab` | Switch active pane |
| `Ctrl+H` | Toggle hidden files |
| `Alt+Return` | Properties |

All shortcuts are customizable in the Settings dialog.

## Project Structure

```
fm-qt/
├── CMakeLists.txt
├── README.md
├── README-zh.md                   # Chinese README
├── REQUIREMENTS.md                # Requirements specification
├── ARCHITECTURE.md                # Architecture design document
├── src/
│   ├── main.cpp                   # Entry point
│   ├── app/                       # Application init, single instance
│   ├── core/                      # Config, clipboard, favorites, session, shortcuts, volume manager
│   ├── dialogs/                   # Settings, conflict, properties, open-with dialogs
│   ├── filelist/                  # File list model, view, sort proxy
│   ├── fileops/                   # File operations, progress, trash
│   ├── panel/                     # Pane container, panel widget, tab bar
│   └── ui/                        # Main window
└── translations/                  # Chinese/English translations (.ts / .qm)
```

## Contributing

Issues and Pull Requests are welcome.

- Ensure the code compiles successfully before submitting
- Update [REQUIREMENTS.md](REQUIREMENTS.md) and [ARCHITECTURE.md](ARCHITECTURE.md) when adding features
- Add translations under `translations/` for any user-visible text changes

## License

This project is licensed under **GPL v3**, consistent with the Qt6 open-source license. See [LICENSE](LICENSE).
