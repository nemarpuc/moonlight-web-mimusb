moonlight-web-mimusb for Windows (unofficial add-on for Moonlight-Web)

Install
  1. Install the official MoonlightWeb first (the add-on replaces its files).
  2. Unpack this zip anywhere and run installer.exe. It asks for administrator rights.
  3. Press Install. The installer stops MoonlightWeb, keeps every file it replaces as
     *.official, and copies the build in "bin".
  4. In the same window, fill in the board's settings and press "Save mimusb.json"
     (it goes beside settings.json in %APPDATA%\MoonlightWeb\MoonlightWeb).
     Then start MoonlightWeb.

Undo: run installer.exe and press "Back to official".
Update: download the newest release of this repository and run its installer.exe.
        Your mimusb.json stays.

The board must run the firmware of the libmimusb version named in the release notes.
Licenses and the way to get the source: the "licenses" folder, and "licenses" inside "bin".
Project: see the repository this was downloaded from.
