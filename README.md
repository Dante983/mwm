# mwm - minimal window manager for macOS

A DWM-inspired tiling window manager for macOS. Follows the suckless philosophy:
simple, minimal, and hackable.

```
    ┌──────────────────┬─────────────────┐
    │                  │                 │
    │                  │     Stack 1     │
    │                  │                 │
    │     Master       ├─────────────────┤
    │                  │                 │
    │                  │     Stack 2     │
    │                  │                 │
    └──────────────────┴─────────────────┘
```

## Features

- **Tiling layouts** - master/stack like DWM, monocle, floating
- **Tag-based workspaces** - 9 tags (like DWM's view system)
- **Keyboard-driven** - all operations via hotkeys
- **Minimal footprint** - ~600 lines of C, ~55KB binary
- **Suckless philosophy** - configure by editing `config.h` and recompiling
- **Native macOS** - uses Accessibility API, no dependencies

## Requirements

- macOS 12.0+ (Monterey or later)
- Xcode Command Line Tools
- Accessibility permissions

## Quick Start

```bash
# 1. Build
make

# 2. Grant accessibility permissions (will prompt on first run)
./mwm

# 3. Install system-wide (optional)
sudo make install

# 4. Start at login (optional)
make enable
```

## Permissions

mwm requires Accessibility permissions to control windows:

1. Run `mwm` - it will trigger a permission prompt
2. Or manually: System Settings → Privacy & Security → Accessibility
3. Add `mwm` (or Terminal if running from there) to allowed apps
4. Restart mwm

## Default Keybindings

All keys use **Option (⌥)** as the modifier. Edit `config.h` to change to Command.

### Window Management

| Key | Action |
|-----|--------|
| `⌥ + j` | Focus next window |
| `⌥ + k` | Focus previous window |
| `⌥ + ⇧ + j` | Swap with next window |
| `⌥ + ⇧ + k` | Swap with previous window |
| `⌥ + ⇧ + c` | Close focused window |
| `⌥ + Tab` | Focus last window |

### Layout

| Key | Action |
|-----|--------|
| `⌥ + h` | Shrink master area |
| `⌥ + l` | Expand master area |
| `⌥ + i` | Add window to master |
| `⌥ + d` | Remove window from master |
| `⌥ + t` | Tiled layout |
| `⌥ + m` | Monocle (fullscreen) layout |
| `⌥ + f` | Floating layout |
| `⌥ + Space` | Cycle layouts |
| `⌥ + ⇧ + Space` | Toggle focused window floating |

### Tags (Workspaces)

| Key | Action |
|-----|--------|
| `⌥ + 1-9` | View tag 1-9 |
| `⌥ + ⇧ + 1-9` | Move window to tag 1-9 |
| `⌥ + Ctrl + 1-9` | Toggle tag view (view multiple) |

### Other

| Key | Action |
|-----|--------|
| `⌥ + Return` | Launch Terminal |
| `⌥ + ⇧ + q` | Quit mwm |

## Configuration

Like DWM, configuration is done by editing `config.h` and recompiling:

```bash
# Edit configuration
vim config.h

# Rebuild
make clean && make

# Reinstall if needed
sudo make install
```

### Key Configuration Options

```c
/* Master area size (0.05 to 0.95) */
#define DEFAULT_MFACT   0.55f

/* Windows in master area */
#define DEFAULT_NMASTER 1

/* Gap between windows */
static const unsigned int gappx = 10;

/* Modifier key: Mod1 = Option, Mod4 = Command */
#define MODKEY Mod1
```

### Adding Custom Apps

Edit the `termcmd` or add new commands:

```c
static const char *termcmd[] = { "/Applications/Alacritty.app", NULL };
static const char *browsercmd[] = { "/Applications/Firefox.app", NULL };
```

Then add keybindings:

```c
{ MODKEY,           Key_B,      spawn,          {.v = browsercmd} },
```

### Window Rules

Force apps to specific tags or floating:

```c
static const Rule rules[] = {
    /* app                  tag     floating */
    { "Spotify",            1 << 8, 0 },  /* always on tag 9 */
    { "Calculator",         0,      1 },  /* always floating */
};
```

## Architecture

```
mwm.c     - Main source (~600 lines)
config.h  - Configuration (keybindings, rules, appearance)
Makefile  - Build system
```

### How It Works

1. Uses **CGEventTap** to capture global hotkeys
2. Uses **AXUIElement** (Accessibility API) to query/move/resize windows
3. Polls every second for new/closed windows
4. Maintains a linked list of managed windows with tags

### Limitations

- Cannot capture keys if app has "Secure Input" (like password fields)
- Some apps may not respond well to programmatic resizing
- Menu bar and Dock space is approximated (not perfectly detected)

## Troubleshooting

### "Failed to create event tap"
- Accessibility permissions not granted
- Check System Settings → Privacy & Security → Accessibility

### Windows not tiling
- Window might be in floating rules
- App might not support resizing (some Electron apps)
- Try `⌥ + ⇧ + Space` to toggle floating

### Hotkeys not working
- Check if another app is using the same hotkeys
- Try a different modifier (Command instead of Option)

## Philosophy

mwm follows the suckless philosophy:

1. **Simple** - One source file, minimal abstractions
2. **Minimal** - Does tiling and nothing else
3. **Hackable** - Modify source to customize
4. **No runtime config** - Edit, compile, run

## License

MIT License - see LICENSE file

## Contributing

Fork, modify, use. This is meant to be personal software that you customize
to your needs. The source is intentionally simple to encourage modification.
