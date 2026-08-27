# RePebble listing draft

## Name

Gym Zones

## Short description

Heart-rate zones, rest timer, workout time, and resting PPG-HRV for Pebble Time 2.

## Description

Gym Zones is a focused strength-training companion for Pebble Time 2. Start and
finish workouts manually, see current BPM and a rolling one-hour graph, track a
stable heart-rate zone, and run a configurable between-set rest timer. The app
can reopen itself when rest ends and preserves a workout for ten minutes after
you exit.

Outside a workout, take a one-minute resting PPG-RMSSD measurement and compare
it with a personal seven-day baseline. HRV is an optical wellness estimate, not
a medical measurement or readiness score.

Gym Zones has no account, server, GPS, or network service and does not transmit
workout or sensor data. Settings stay locally on the watch and in Clay on the
phone; Pebble Health synchronization, if enabled, is managed by PebbleOS.
Assign Gym Zones to `Settings → Quick Launch → Tap Up` for direct access.

## Compatibility

- Pebble Time 2 (`emery`) only
- PebbleOS 4.32+
- Pebble Health required for HR and HRV

## Version

1.0.0

## Suggested screenshots

1. `screenshots/first-launch.png`
2. `screenshots/z3.png`
3. `screenshots/z5.png`
4. `screenshots/rest.png`
5. `screenshots/finish-confirm.png`
6. `screenshots/summary.png`
7. `screenshots/auto-finish.png`
8. `screenshots/wakeup-foreground.png`
9. `screenshots/hrv-unsupported.png`

Replace or supplement the HRV screenshot after the physical Pebble Time 2
sensor validation described in `VALIDATION.md`.

## Privacy

Heart-rate and workout data remain on the watch. Raw PPI is RAM-only and is not
persisted. Settings are stored locally on the watch and in Clay on the phone.
The app does not transmit sensor or workout data.
