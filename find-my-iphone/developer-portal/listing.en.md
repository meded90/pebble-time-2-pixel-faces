# Find My iPhone

Ring your iPhone directly from Pebble Time 2. Open the watch app, select an iPhone with the Up and Down buttons, then hold Select for 650 ms to send the sound command.

The app runs through Pebble Core on your iPhone and talks directly to Apple over HTTPS. No separate companion app or intermediary server is required. Setup supports trusted-device two-factor authentication, multiple iPhones, automatic system language selection and clear states for offline devices, expired sessions and rate limits.

Your password and two-factor code are used only during sign-in and are not stored. Apple session tokens remain in Pebble Core local storage on the paired iPhone. You can remove the saved session from the settings page at any time.

Important: this is an independent, unofficial utility based on an undocumented Apple service. It is not affiliated with or endorsed by Apple. Apple may change the service without notice, which can temporarily or permanently stop sign-in or sound requests from working.

Requires Pebble Time 2 (`emery`), Pebble Core on iPhone, an internet connection and an Apple Account with Find My enabled.

## Release notes 0.1.2

- Settings now resume the saved login, 2FA, device-selection, or connected step after Pebble closes the configuration page. Reopen Settings to continue after each action.
- Fixed Apple SRP sign-in so refreshed challenge headers, session IDs, auth attributes, and cookies are carried through authorize, initialization, and completion requests.
- Safe sign-in diagnostics now include the HTTP status and Apple error code without logging passwords, 2FA codes, cookies, or session tokens.
- Real Apple sign-in remains a physical-iPhone validation step; the public browser QEMU is intended only for UI and protocol testing.
