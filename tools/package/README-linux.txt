moonlight-web-mimhid for Linux, x86_64 (unofficial add-on for Moonlight-Web)

Install
  1. Unpack this archive and run ./installer in the unpacked folder.
  2. Press Install. The AppImage in "bin" goes to ~/.local/bin/MoonlightWeb.AppImage with a
     menu entry (an existing file is kept once as MoonlightWeb.AppImage.official).
  3. In the same window, fill in the board's settings and press "Save mimhid.json"
     (it goes to ~/.local/share/MoonlightWeb/MoonlightWeb, beside settings.json).
  4. Add yourself to the serial port's group, then sign in again:
         sudo usermod -aG dialout $USER        (uucp on Arch)

Without a screen: ./installer install, ./installer rollback, ./installer status.
Undo: press "Back to official" (or ./installer rollback).
Update: download the newest release of this repository and run its installer.
        Your mimhid.json stays.

The window needs OpenGL 3 and an X11 or Wayland session.
The board must run the firmware of the libmimhid version named in the release notes.
Licenses and the way to get the source: the "licenses" folder, and /usr/share/doc/moonlight-web-mimhid
inside the AppImage (run the AppImage with --appimage-extract to read it).
