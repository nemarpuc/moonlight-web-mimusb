moonlight-web-mimhid for Windows (unofficial add-on for Moonlight-Web)

Install
  1. Install the official MoonlightWeb first (the add-on replaces its files).
  2. Unpack this zip anywhere and double-click Install.bat.
     It asks for administrator rights, stops MoonlightWeb, keeps every file it replaces
     as *.official, copies the new files, and puts a shortcut on the desktop:
     "MoonlightWeb - back to official build".
  3. Copy mimhid.example.json to mimhid.json in %APPDATA%\MoonlightWeb\MoonlightWeb
     (beside settings.json), edit it, and start MoonlightWeb.

Undo: the desktop shortcut, or rollback.ps1 in this folder.
Update: download the newest release of this repository and run Install.bat again.

The board must run the firmware of the libmimhid version named in the release notes.
Licenses and the way to get the source: the "licenses" folder inside "app".
Project: see the repository this was downloaded from.
