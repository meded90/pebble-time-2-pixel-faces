# RePebble deployment package

**English** · [Русский](README.ru.md)

Prepared listing materials for the Find My iPhone watchapp on `emery`.

## Required files

- `icons/icon-48.png` and `icons/icon-144.png`;
- `banner/marketing-banner-720x320.png`;
- five native 200×228 screenshots under `screenshots/emery/`;
- [`../../dist/candidates/find-my-iphone-0.1.1-emery.pbw`](../../dist/candidates/find-my-iphone-0.1.1-emery.pbw);
- `description.en.txt`, `release-notes.en.txt`, and `metadata.json`.

Additional assets include menu/store icons, Russian listing copy, expanded
`listing.*.md` documents, and `store-assets-preview.png`.

## Before publication

1. Replace the support-email `TODO` in `metadata.json` or in the portal.
2. Confirm that the source URL is publicly reachable.
3. Test sign-in, 2FA, and a real sound request on a physical iPhone and Pebble
   Time 2.
4. Review `SECURITY.md`; the app uses an undocumented Apple API and stores
   session tokens in Pebble Core `localStorage`, not Keychain.
5. Start with a hidden/private listing. Publishing is a separate public action.

Regenerate store graphics with:

```sh
swift tools/build-store-assets.swift
```
