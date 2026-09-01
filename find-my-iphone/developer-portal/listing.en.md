# Find My iPhone

Ring your iPhone directly from Pebble Time 2. Open the watch app, select an iPhone with the Up and Down buttons, then hold Select for 650 ms to send the sound command.

The app runs through Pebble Core on your iPhone and talks directly to Apple over HTTPS. No separate companion app or intermediary server is required. Setup supports trusted-device two-factor authentication, multiple iPhones, automatic system language selection and clear states for offline devices, expired sessions and rate limits.

Your password and two-factor code are used only during sign-in and are not stored. Apple session tokens remain in Pebble Core local storage on the paired iPhone. You can remove the saved session from the settings page at any time.

Important: this is an independent, unofficial utility based on an undocumented Apple service. It is not affiliated with or endorsed by Apple. Apple may change the service without notice, which can temporarily or permanently stop sign-in or sound requests from working.

Requires Pebble Time 2 (`emery`), Pebble Core on iPhone, an internet connection and an Apple Account with Find My enabled.

## Release notes 0.1.0

Initial preview release with direct Find My sound requests, trusted-device 2FA, multiple-iPhone selection, a 650 ms hold guard, multilingual UI and explicit offline/auth/rate-limit states.
