moonlight-web-mimhid for macOS, Apple silicon (unofficial add-on for Moonlight-Web)

Install
  1. Unpack this zip and open "installer" in the unpacked folder. It is signed ad hoc, so macOS
     asks once: allow it in System Settings > Privacy & Security (or right-click > Open).
  2. Press Install. MoonlightWeb.app in "bin" goes to /Applications (an existing app is kept once
     as MoonlightWeb.app.official).
  3. In the same window, fill in the board's settings and press "Save mimhid.json"
     (it goes to ~/Library/Application Support/MoonlightWeb/MoonlightWeb, beside settings.json).
     Start MoonlightWeb.

Without a screen: ./installer install, ./installer rollback, ./installer status (in Terminal).
Undo: press "Back to official".
Update: download the newest release of this repository and run its installer.
        Your mimhid.json stays.

The board must run the firmware of the libmimhid version named in the release notes.
Licenses and the way to get the source: the "licenses" folder, and "licenses" inside the app.
