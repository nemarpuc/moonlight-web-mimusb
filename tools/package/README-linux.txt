moonlight-web-mimhid for Linux, x86_64 (unofficial add-on for Moonlight-Web)

Install
  1. Unpack this archive, open a terminal in the unpacked folder and run:
         bash install.sh install
     The AppImage goes to ~/.local/bin/MoonlightWeb.AppImage with a menu entry (an existing
     file is kept once as MoonlightWeb.AppImage.official).
  2. Add yourself to the serial port's group, then sign in again:
         sudo usermod -aG dialout $USER        (uucp on Arch)
  3. Copy mimhid.example.json to mimhid.json in ~/.local/share/MoonlightWeb/MoonlightWeb
     (beside settings.json), edit it, and start MoonlightWeb.

Undo: bash install.sh rollback
Update: bash install.sh update

The board must run the firmware of the libmimhid version named in the release notes.
Licenses and the way to get the source: /usr/share/doc/moonlight-web-mimhid inside the AppImage
(run the AppImage with --appimage-extract to read it).
