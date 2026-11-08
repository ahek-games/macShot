# MacShot 📸

A lightweight, modern macOS-style screenshot utility built for Ubuntu and Linux desktops using **Qt 6**, **C++17**, and **XDG Desktop Portals**.

MacShot brings the fluid, non-intrusive macOS screenshot workflow to Linux: capture full screens or custom regions, see a floating thumbnail card in the corner of your screen, drag-and-drop or edit/annotate with rich markup tools, and copy straight to clipboard without cluttering your filesystem.

---

## ✨ Features

- **Fluid macOS Animations**:
  - **Camera Shutter Flash**: A crisp translucent shutter flash illuminates over the captured area the moment a screenshot is taken.
  - **Corner Warp Transition**: The captured screenshot animates smoothly from the selection's origin position on the screen, scaling down with a subtle bounce (`QEasingCurve::OutBack`) into your preferred corner dock.
  - **Fade & Dismiss**: The thumbnail gently glides in and fades out upon expiration or dismissal without harsh jumps.

- **macOS Floating Thumbnail & Multi-Shot Stacking**:
  - Appears smoothly in your configured screen corner upon capture.
  - **Stacking System**: Take multiple screenshots in succession without waiting! They automatically stack behind one another with a 3D-card deck visual effect and badge counter.
  - Each new screenshot resets the auto-dismiss timer and plays a playful bump/pulse animation.
  - **Multi-Image Drag & Drop**: Dragging the stack drops **all** captured images together into file managers or upload dropzones.
  - Hovering pauses the countdown timer.
  - Right-click dismisses the stack instantly.
  - Left-click opens editor windows for the stacked screenshots.
  - Seamless drag-and-drop into file managers (Nautilus), web browsers (including Snap Firefox & Chromium), chat inputs (Discord, Slack, AI web chat), and document editors.
  - **Auto-Cleaning Temp Storage (`~/.macshot_temp`)**: Drag-and-drop files are saved in `~/Pictures/.macshot_temp`, which is accessible across Ubuntu Snap sandbox boundaries. Screenshots older than 24 hours are automatically purged on startup and every 4 hours, ensuring zero accumulation and no huge disk pileup over time.

- **Multi-Monitor Region Selection**:
  - Smooth darkened overlay across all connected monitors.
  - Crosshair cursor with dimensional magnifier tooltip.
  - `Enter` captures the monitor under the cursor; `Shift + Enter` captures the entire desktop spanning all displays; `Escape` cancels.

- **Built-in Markup & Annotation Editor**:
  - Tools: **Arrow**, **Rectangle**, **Freehand Pen**, **Text Box**, **Pixelate**, and **Blur**.
  - Color palette selection and adjustable stroke widths.
  - Undo / Redo history support (`Ctrl+Z`, `Ctrl+Y`).
  - Single-click **Copy to Clipboard** and **Save As...** (`Ctrl+S`).

- **Ubuntu & Wayland/X11 Native Integration**:
  - Global shortcut integration via GNOME `gsettings` (`media-keys custom-keybindings`).
  - Native screenshot capture through `org.freedesktop.portal.Screenshot`.
  - System Tray menu for quick access, triggers, and configuration.
  - Single-instance daemon architecture via `QLocalServer`/`QLocalSocket` (trigger new captures using `macshot --capture`).
  - Optional auto-start on desktop login via XDG autostart.

---

## 🛠️ Prerequisites & Dependencies

To build MacShot on Ubuntu (22.04 LTS / 24.04 LTS or newer) or Debian-based distributions:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libgl1-mesa-dev \
    xdg-desktop-portal \
    gsettings-desktop-schemas
```

---

## 🚀 Building & Packaging

### 1. Build from Source

```bash
# Clone the repository
git clone https://github.com/your-username/MacShot.git
cd MacShot

# Configure and compile
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The compiled binary will be placed at `build/macshot`.

### 2. Run Locally

```bash
./build/macshot
```

To trigger a screenshot from the command line while the daemon is running:
```bash
./build/macshot --capture
```

### 3. Generate a Debian Package (`.deb`)

You can generate a standalone `.deb` package using `cpack`:

```bash
cd build
env -u LD_LIBRARY_PATH -u GTK_PATH -u GIO_MODULE_DIR cpack -G DEB
```

Install the generated `.deb`:
```bash
sudo apt install ./macshot_1.0.0_amd64.deb
```

---

## ⚙️ Configuration & Shortcuts

Open the **Settings** dialog from the system tray icon to configure:
- **Global Shortcut**: Default is usually `Print` or custom combinations (e.g. `Ctrl+Shift+4`).
- **Thumbnail Corner**: Choose Bottom-Right, Bottom-Left, Top-Right, or Top-Left.
- **Dismiss Delay**: How many seconds before the thumbnail auto-dismisses (1–30s).
- **Launch at Startup**: Automatically registers in `~/.config/autostart/macshot.desktop`.

---

## 📁 Project Architecture

```
MacShot/
├── src/
│   ├── CMakeLists.txt        # Build configurations and CPack deb rules
│   ├── macshot.desktop       # Desktop launcher specification
│   └── main.cpp              # Core application code
│       ├── SettingsManager          # QSettings, autostart & GNOME shortcut binding
│       ├── AnnotationCanvas         # Interactive canvas with undo/redo & draw tools
│       ├── PreviewEditorDialog      # Annotation dialog with toolbars and action buttons
│       ├── FloatingThumbnail        # Floating corner card with animations
│       ├── RegionSelectionOverlay   # Multi-display crop overlay
│       ├── SettingsDialog           # Configuration GUI
│       └── ScreenshotDaemon         # Tray icon, single-instance socket & portal caller
└── README.md
```

---

## 📄 License

Distributed under the MIT License. Feel free to use and modify!
