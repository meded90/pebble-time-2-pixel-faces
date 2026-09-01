# Repository agent instructions

Read `README.md`, `TESTING.md`, `projects.json`, and the requested project's
`README.md` and `CHANGELOG.md` before working on a Pebble application.

## Showing an application for testing

When the user asks to show, open, run, or launch a watchface/watchapp for
testing, use the browser-first workflow in `TESTING.md`:

1. build current source or resolve the requested PBW from `projects.json`;
2. open `https://ericmigi.github.io/pebble-qemu-wasm/`;
3. boot Emery / Pebble Time 2;
4. upload the local PBW and leave the interactive browser result available.

Use the standard Pebble SDK emulator only after a concrete browser boot,
WebAssembly, upload, install, or runtime failure, or when the requested behavior
requires capabilities not represented by the browser emulator. State the reason
for fallback. Do not claim physical-hardware validation from either emulator.

## Versions and release notes

Treat release notes as part of every version change, including release
candidates and unpublished versions. Without waiting for a separate user
request, the same change must:

1. update the version in the project's `package.json` and `projects.json`;
2. prepend a `## <version> — YYYY-MM-DD` entry to the project's `CHANGELOG.md`;
3. describe user-visible changes, compatibility or setup changes, and important
   limitations with concrete English bullet points;
4. preserve every older changelog entry;
5. update artifact metadata only when the matching PBW actually exists.

Never finish or hand off a version bump without its changelog entry. If the
version was already changed when work began and its entry is missing, reconstruct
the notes from the actual diff and validation evidence. Do not invent changes or
claim publication. Run `npm run projects:check`; it enforces a changelog entry
with content for every project's current source version.

Preserve unrelated user changes. Do not stage, commit, push, publish, deploy, or
submit an Appstore listing unless the user explicitly requests that action.
