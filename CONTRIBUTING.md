# Contributing

**English** · [Русский](CONTRIBUTING.ru.md)

Thank you for improving the Pebble Time 2 projects in this repository. Keep
changes scoped to one watchface, watchapp, or shared repository concern whenever
possible.

## Development setup

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Build all watchfaces with `make build`. Build public-SDK watchapps with
`make standard-apps`. Voice Drop belongs to `make patched-apps` because its
current source requires the patched PebbleOS SDK documented in
[`voice-drop/README.md`](voice-drop/README.md).

## Add or update a Pebble project

1. Keep each project in its own directory with its own `package.json`, UUID,
   version, source, resources, and build output.
2. Never change the UUID of an already published application. RePebble treats a
   different UUID as a different application.
3. Make `README.md` the canonical English guide and `README.ru.md` the Russian
   companion. Keep commands, requirements, privacy notes, and validation
   boundaries equivalent.
4. Add a native preview. Emery screenshots must be exactly 200×228; Gabbro
   screenshots must be 260×260.
5. Update [`projects.json`](projects.json). It is the source of truth for the
   project list, source version, platforms, preview, build group, store status,
   and retained artifact.
6. Run `npm run projects:sync`. Do not edit generated catalog blocks, Makefile
   project lists, `dist/README*.md`, or `dist/SHA256SUMS` by hand.
7. Add or update project-specific tests and validation notes.

## Release artifacts

The status belongs to the PBW binary, not just to the project:

- `dist/published/` contains a retained PBW whose `versionLabel` matches a
  verified public RePebble release; byte identity with the store download is
  claimed only when separately checked;
- `dist/candidates/` contains installable builds that are not publicly verified;
- `dist/experimental/` contains prototypes with explicit compatibility limits.

Use a versioned filename such as `project-name-1.2.3.pbw`. Record the same path
and version in `projects.json`, then run `npm run projects:sync`. The generator
checks the PBW `appinfo.json` version and rewrites `dist/SHA256SUMS`.

Do not mark a project or artifact as published merely because a listing form is
prepared or a PBW exists. Add the public URL and `publishedVersion` only after
the Appstore page is reachable and the version is visible there.

## Required checks

Run checks proportionate to the change:

```bash
npm run projects:check
git diff --check
make -n build
make -n standard-apps
```

For every changed PBW:

```bash
unzip -t path/to/project-version.pbw
unzip -p path/to/project-version.pbw appinfo.json
```

Run the affected project's own tests and build. Emulator screenshots are useful
evidence, but health sensors, microphone behavior, phone integrations, wakeups,
and external services may require physical hardware. State untested coverage
explicitly.

## Store assets

Before publication, provide the sizes required by RePebble, including an Emery
200×228 screenshot, app/watchface icons, and a 720×320 marketing banner when the
listing requires one. Keep listing copy, release notes, privacy disclosures, and
source links next to the project.

## Security and privacy

- Never commit credentials, session tokens, private service URLs, `.env` files,
  Apple account data, Telegram bot tokens, or OpenAI credentials.
- Use placeholders in reusable documentation.
- Document every network destination, permission, stored value, retention rule,
  and hardware or service dependency.
- Treat reverse-engineered and private APIs as experimental and describe their
  failure and publication boundaries clearly.

## Before handing off a change

- Review the complete working-tree diff and preserve unrelated user changes.
- Confirm that English and Russian documentation still agree.
- Confirm that local links and preview paths resolve.
- Do not commit, push, publish, or deploy unless that action was explicitly
  requested.
