# Release artifacts

**English** · [Русский](README.ru.md)

PBW files are separated by the status of the binary itself. “Published-version” means that `versionLabel` matches a verified public release; byte identity with the Appstore download is not implied unless separately documented. A newer local binary remains a release candidate.

| Project | Artifact | Version | Binary status | Store status |
| --- | --- | --- | --- | --- |
| [Find My iPhone](../find-my-iphone/) | [candidates/find-my-iphone-0.1.1-emery.pbw](candidates/find-my-iphone-0.1.1-emery.pbw) | 0.1.1 | Release candidate | Not published |
| [Gym Zones](../gym-zones/) | [candidates/gym-zones-1.0.0.pbw](candidates/gym-zones-1.0.0.pbw) | 1.0.0 | Release candidate | Not published |
| [Info Tiles](../info-tiles/) | [candidates/info-tiles-1.0.0.pbw](candidates/info-tiles-1.0.0.pbw) | 1.0.0 | Release candidate | Not published |
| [meded90](../meded90/) | [candidates/meded90-1.0.0.pbw](candidates/meded90-1.0.0.pbw) | 1.0.0 | Release candidate | Not published |
| [Wrist Agent](../wrist-agent/) | [candidates/wrist-agent-1.0.0.pbw](candidates/wrist-agent-1.0.0.pbw) | 1.0.0 | Release candidate | Not published |
| [Zodiac: Aquarius](../zodiac-aquarius/) | [candidates/zodiac-aquarius-1.3.0.pbw](candidates/zodiac-aquarius-1.3.0.pbw) | 1.3.0 | Release candidate | Not published |
| [Zodiac: Gemini](../zodiac-gemini/) | [candidates/zodiac-gemini-1.4.0.pbw](candidates/zodiac-gemini-1.4.0.pbw) | 1.4.0 | Release candidate | [1.3.1](https://apps.repebble.com/zodiac-gemini_a1c61f1227144535accbf53c) |
| [Voice Drop](../voice-drop/) | [experimental/voice-drop-prototype-0.1.0.pbw](experimental/voice-drop-prototype-0.1.0.pbw) | 0.1.0 | Experimental prototype | Not published |
| [Codex Weekly](../codex-weekly/) | [published/codex-weekly-1.0.7.pbw](published/codex-weekly-1.0.7.pbw) | 1.0.7 | Published-version binary | [1.0.7](https://apps.repebble.com/codex-weekly_27f48d86803e471a83b93dfe) |
| [Flip Board](../flip-board/) | [published/flip-board-1.2.1.pbw](published/flip-board-1.2.1.pbw) | 1.2.1 | Published-version binary | [1.2.1](https://apps.repebble.com/flip-board_d9b87c9f10a74d718db82b06) |
| [Mosaic Grid](../mosaic-grid/) | [published/mosaic-grid-1.0.2.pbw](published/mosaic-grid-1.0.2.pbw) | 1.0.2 | Published-version binary | [1.0.2](https://apps.repebble.com/mosaic-grid_d2ccde5d2187490085b44f8e) |
| [Starry Digits](../starry-digits/) | [published/starry-digits-1.1.0.pbw](published/starry-digits-1.1.0.pbw) | 1.1.0 | Published-version binary | [1.1.0](https://apps.repebble.com/starry-digits_bacf5a80f08845558f44cf65) |

`experimental/voice-drop-prototype-0.1.0.pbw` is the legacy phone-recording prototype and is not compatible with the current Voice Drop 0.2.0 source.

Verify every file against [SHA256SUMS](SHA256SUMS). Regenerate this directory metadata with `npm run projects:sync`.
