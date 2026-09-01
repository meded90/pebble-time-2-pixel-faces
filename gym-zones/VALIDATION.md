# Gym Zones 1.0.0 — validation record

Date: 2026-08-27

Target: Pebble Time 2 (`emery`, 200×228)

Toolchain: Pebble SDK 4.33.1, pebble-tool 5.0.39

## Confirmed locally

- Clean native build for `emery`; the package manifest identifies a watchapp,
  not a watchface, and requests `health` plus `configurable` capabilities.
- Host tests cover zone formulas and boundaries, two-reading hysteresis, BPM
  filtering, duration formatting, PPI gates/RMSSD, the seven-value median,
  session close/resume/auto-finish boundaries, rest generation/cookies and
  one-shot alerts, the graph-ring contract for gaps/source priority, and HRV
  phase transitions. Production uses a smaller persistent chart layout; its
  indexing/backfill path was source- and emulator-reviewed but is not directly
  compiled into the host graph test.
- The host suites pass with `-Wall -Wextra -Werror -pedantic` and with
  AddressSanitizer plus UndefinedBehaviorSanitizer.
- Clay settings were sent through the emulator configuration/AppMessage path.
  The native app accepted a valid complete transaction and rendered stable Z3
  and Z5 states from injected HR events.
- Button walkthroughs in the Emery emulator covered manual start, rest, `+30`,
  finish confirmation/cancellation, manual finish, saved summary, HRV view, and
  the unsupported-HRV state.
- A live exit to the launcher exposed `TIME` in AppGlance; reopening Gym Zones
  before the ten-minute deadline resumed the same session and continuous time.
- Advancing only the Emery emulator clock beyond a later ten-minute deadline
  materialized a native `AUTO FINISH` summary at the anchored deadline.
- Rest crossed zero into `+MM:SS`; after `+30` rescheduling, switching to a
  different watchapp and waiting for the deadline brought Gym Zones back to the
  foreground with the preserved overtime screen.
- Visual inspection confirms a 69 px graph, no horizontal `1–5` strip or
  triangle, an isolated 20 px right zone column, local active-cell background,
  graph gaps, two rows of six dots filling top-to-bottom by column and then
  left-to-right, and `TIME` rather than `GYM 47m`.
- AppGlance reload succeeds with the application default icon.

## Release artifact

- Path: `../dist/candidates/gym-zones-1.0.0.pbw`
- Size: 679614 bytes
- SHA-256: `c321b1fae53615598f1449e6bb36400bfa242ecb92abf4e0b469f5cbdf70aa17`
- The release copy is byte-identical to the final clean-build PBW and passes
  `unzip -t` with an Emery binary, resources, Clay JavaScript and manifests.

Native 200×228 captures are stored in [`screenshots/`](screenshots/). The Z3,
Z5, below-Z1, no-HR, column-major dots, rest, rest-overtime, Wakeup foreground,
confirmation, summary, resume, auto-finish and unsupported-HRV captures come
from the SDK emulator. The interactive Steel Bands reference also contains
result, measuring, low-signal, no-signal and health-off design states.

## Not yet proven on physical hardware

The Emery emulator cannot validate the optical sensor. Before publication,
test these items on a real Pebble Time 2 with firmware 4.32 or newer:

- actual HR cadence with a requested 5 s period and reset to period `0` on exit;
- `HealthEventHRVUpdate`, 1 s PPI requests, still/motion/poor-contact captures,
  and request cleanup for every exit path;
- `Tap Up` Quick Launch behavior on the installed firmware;
- WakeupService foreground launch on the installed physical firmware and its
  physical one-shot vibration after leaving the app during rest;
- battery use during a representative workout and absence of elevated sensor
  sampling after closing Gym Zones.

Native HRV result/measuring/low-signal screenshots are consequently not treated
as sensor evidence: this Emery build reports the HRV request as unsupported.
The Wakeup foreground path also succeeds in Emery and its one-shot race is
covered by the shared production state-machine tests, but the physical
vibration still requires an end-to-end watch run.

## Release gate

The PBW, listing draft and screenshots may be reviewed now. Do not submit the
RePebble form until the physical-device checks above are recorded and the user
gives a separate final publication confirmation. This work does not stage,
commit, push, deploy, or publish anything.
