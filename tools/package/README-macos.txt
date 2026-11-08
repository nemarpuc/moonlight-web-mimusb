moonlight-web-mimhid for macOS, Apple silicon (unofficial add-on for Moonlight-Web)

Install
  1. Unpack this zip, open Terminal in the unpacked folder and run:
         bash install.sh install
     MoonlightWeb.app goes to /Applications (an existing one is kept once as
     MoonlightWeb.app.official).
  2. Copy mimhid.example.json to mimhid.json in
     ~/Library/Application Support/MoonlightWeb/MoonlightWeb (beside settings.json),
     edit it, and start MoonlightWeb. The app is signed ad hoc: allow it in
     System Settings > Privacy & Security.

Undo: bash install.sh rollback
Update: bash install.sh update

The board must run the firmware of the libmimhid version named in the release notes.
Licenses and the way to get the source: inside MoonlightWeb.app/Contents/Resources/licenses.
