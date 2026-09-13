Beastcaller - build your own Windows .exe
==========================================

This folder is a ready-to-build Electron project. Because your PC has
normal internet access (this sandbox doesn't), building has to happen
on your machine - it only takes a couple of minutes.

REQUIREMENTS
------------
- Node.js (you already have v24 installed - nothing to do here)
- Windows with internet access (to download Electron itself, ~150-200MB,
  and Three.js is loaded live from a CDN when the game runs)

STEPS
-----
1. Open PowerShell (or Command Prompt) and go into this folder, e.g.:

     cd $HOME\source\beastcaller-electron

2. Install dependencies (downloads Electron + electron-builder):

     npm install

3. Build the real .exe:

     npm run build

   This creates:  dist\Beastcaller.exe
   It's a single portable executable - no installer, just double-click
   it to play. You can copy that one file anywhere.

   (Optional) To just try the game without building an .exe first, run:

     npm start

WHAT'S INSIDE
-------------
- index.html   -> the actual game (3D creatures via Three.js, gacha
                   summon system, team building, expedition battles)
- main.js      -> the tiny Electron wrapper that opens index.html in
                   its own window instead of a browser tab
- package.json -> project + build configuration

If `npm run build` complains about anything, the most common fix is
just re-running `npm install` once more (first-time downloads
occasionally need a retry), then `npm run build` again.
