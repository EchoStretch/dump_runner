# Homebrew Dump Runner

Run homebrew games on your PS5 using the Kstuff, Websrv payloads and Homebrew Launcher.

---

### ✅ Installation & Launch

1. **Copy your homebrew folder** to:
   - `/data/homebrew/`  
   - `/mnt/usb#/homebrew/` *(replace `#` with your USB number, e.g., `usb0`, `usb1`, etc.)*
   - `/mnt/ext#/homebrew/` *(replace `#` with your EXT number, e.g., `ext0`, `ext1`, etc.)*

2. **Inside each homebrew game folder**, add:
   - `dump_runner.elf`
   - `homebrew.js`

3. **Install the Homebrew Launcher** and send the Websrv payload to your console:  
   👉 [Websrv v0.23.1](https://github.com/ps5-payload-dev/websrv/releases/tag/v0.23.1)

4. **Open the Homebrew Launcher.**  
   With the USB plugged in, your homebrew game should appear.  
   **Select a homebrew game and run it.**

---

### 📌 Example Folder Structure

`/data/homebrew/MyHomebrewGame/`, `/mnt/usb0/homebrew/MyHomebrewGame/` or `/mnt/ext0/homebrew/MyHomebrewGame/`
   - `dump_runner.elf`
   - `homebrew.js`
   - `[other homebrew game files...]`

---
