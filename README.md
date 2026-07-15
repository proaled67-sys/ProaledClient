# 🌙 ProaledClient

A custom DDNet client with a pink & black aesthetic, moon branding, and a packed set of features merged from the best community clients.

## Features

### From BestClient
- 🎨 HUD Editor — customize every element of your HUD
- ⚡ Fast Actions — one-click community features
- 🏃 Fast Practice — speed-training helpers
- 🎵 Music Player — in-game music with album art color sync
- 💎 Crystal Laser — stunning rifle/shotgun visuals
- 🎆 3D Particles — animated particle system
- 🖥️ Admin Panel — pink-themed admin controls
- 📷 Chat Media Previews — images/GIFs inline in chat
- 🔮 R-Jelly & R-Trail — visual effects
- 📊 Visualizer — audio visualizer
- 🎯 Finish Prediction Bar
- 🌐 Custom Aspect Ratio support
- 🕵️ Streamer mode (hide sensitive info)
- 🔄 Hook combo tracker

### From RushieClient (exclusive additions)
- 🗣️ **Voice Chat** — in-game proximity voice with noise suppression
- 💬 **Chat Bubbles** — speech bubbles above player heads
- 🎡 **Bind Wheel** — radial bind menu
- 🎸 **Music Island** — themed music experience
- 👤 **Player Menu** — context menu on player click
- 📁 **Settings Profiles** — save/load full config profiles
- 📐 **Edge Helper** — visual edge detection

### ProaledClient Originals
- 🌙 **Moon Logo** — custom icon & branding
- 🩷 **Pink & Black theme** — all menus use a pink accent on black
- Named `ProaledClient` throughout

## Building (Windows)

### Option A — GitHub Actions (recommended, no setup needed)
1. Push this repo to GitHub
2. Go to **Actions → Build ProaledClient (Windows)**
3. Click **Run workflow**
4. Download `ProaledClient-win64.zip` from the run artifacts

### Option B — Build locally on Windows
```bat
git clone <your-repo> ProaledClient --recursive
cd ProaledClient
git clone https://github.com/ddnet/ddnet-libs.git
cmake -B build -A x64 -DVULKAN=ON -DEXCEPTION_HANDLING=ON
cmake --build build --config Release
```
The executable will be at `build/Release/ProaledClient.exe`.

## Default Config Highlights

| Setting | Value |
|---|---|
| Admin panel active tab | Pink (`#B40069`) |
| Admin panel hover tab | Hot pink (`#FF69B4`) |
| Client name | `ProaledClient` |
| Version | `1.0.0` |

## Color Scheme

| Role | Color |
|---|---|
| Primary accent | `#FF69B4` (Hot Pink) |
| Secondary | `#FF1493` (Deep Pink) |
| Background | `#000000` (Black) |
| Background tint | `#1A001A` (Dark Purple-Black) |

## Credits

- **DDNet** — base engine (GPLv2)
- **BestClient** — feature base (GPLv2)
- **RushieClient** — additional features (GPLv2)
- **ProaledClient** — your custom client ❤️

## License

GPLv2 — see `LICENSE` for details.
