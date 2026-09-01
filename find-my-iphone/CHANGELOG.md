# Changelog

## 0.1.4 — 2026-09-01

- Treat Apple's asynchronous HTTP 202 response to the trusted-device notification request as accepted, so the delivered code opens the 2FA entry step instead of returning to the password form.

## 0.1.3 — 2026-09-01

- Explicitly request the trusted-device verification notification after Apple returns the SRP 2FA challenge instead of waiting for a code that was never triggered.
- Add a localized “Send code again” action that reuses the saved 2FA challenge without asking for the Apple Account password again.
- Accept Apple's HTTP 409 response to a valid verification code only when the response also issues a new Apple session token.
- Keep SMS and voice-call verification-code delivery outside this patch; a trusted Apple device is still required.

## 0.1.2 — 2026-09-01

- Resume the saved login, 2FA, device-selection, or connected step whenever Pebble Settings is opened again.
- Stop trying to reopen the configuration WebView automatically after a form submission, which was unreliable on physical iPhones.
- Explain in every supported language that Pebble closes Settings after each action and that the user must reopen it to continue.
- Preserve the latest safe configuration error across Settings reopen and PebbleKit JS restarts; passwords and 2FA codes remain transient.
- Carry Apple's updated `scnt`, session ID, auth attributes, and cookies through every SRP sign-in request instead of reusing stale bootstrap headers.
- Include the safe HTTP status and Apple error code in sign-in diagnostics without logging credentials, cookies, or session tokens.
- Treat public browser QEMU as a UI and protocol test only because its CORS fallback routes web requests through an external proxy; real Apple sign-in remains a physical-iPhone validation step.

## 0.1.1 — 2026-09-01

- Scale the sofa cushion and iPhone to the native 145×99 foreground layer instead of cropping the composition on Pebble Time 2.
- Preserve the complete foreground and transparent safety padding after scaling.

## 0.1.0

- Add the initial Pebble Time 2 release candidate for sending a direct Apple Find My sound request through PebbleKit JS.
- Add trusted-device 2FA, iPhone selection, guarded activation, safe watch states, and local session management.
