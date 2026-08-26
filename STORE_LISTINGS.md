# Appstore listing drafts

## Starry Digits

**Short description**

Hand-drawn luminous digits over a swirling 64-color pixel-art night.

**Full description**

Starry Digits combines large two-row numerals with a moonlit sky built from
bold pixel strokes. The quiet upper-left area keeps the time readable, while
the moon, flowing blue lines and dark village silhouette carry the artwork
across both Pebble Time 2 and Pebble Round 2. Each display uses its own native
background and layout.

The watchface follows the system 12/24-hour preference, updates every minute
and works completely offline. It has no settings, companion app, analytics or
network permissions.

**Permissions:** none.

## Zodiac: Gemini

**Short description**

A vivid Gemini portrait with a large, rounded clock for Pebble Time 2.

**Full description**

Zodiac: Gemini pairs a vivid twin portrait with a large, rounded digital clock
designed specifically for the 200×228 Pebble Time 2 display. Hours and minutes
are stacked in a balanced card in the upper-left corner, leaving the original
illustration clear and expressive.

The watchface follows the system 12/24-hour preference, updates every minute
and works completely offline. It has no settings, companion app, analytics or
network permissions.

**Permissions:** none.

## Mosaic Grid

**Short description**

A bold pixel watchface built from time, date, battery and four disciplined
color blocks.

**Full description**

Mosaic Grid turns the Pebble Time 2 display into a compact modernist poster.
Hours and minutes stay dominant, while the weekday, date and live battery
percentage fit into a strict 200×228 grid. It supports the system 12/24-hour
preference and updates without phone connectivity.

**Permissions:** none.

## Flip Board

**Short description**

A serious four-panel flip clock with date and battery status.

**Full description**

Flip Board brings the calm visual rhythm of a mechanical departure board to
Pebble Time 2. Four high-contrast pixel panels display hours and minutes, with
weekday, date, month and live battery percentage in a narrow information
column. It supports the system 12/24-hour preference and works fully offline.

**Permissions:** none.

## Info Tiles

**Short description**

Time, weather, battery, heart rate and steps in a clear pixel dashboard.

**Full description**

Info Tiles places the information you check most often into a compact,
high-contrast grid designed natively for the 200×228 Pebble Time 2 display.
It shows time, weekday, date, watch battery, current temperature, weather
condition, the latest available heart-rate value and today's step count.

Weather refreshes every 30 minutes through the connected phone. If GPS,
network access or Pebble Health data is unavailable, the affected tile shows
`--` while the rest of the watchface continues to work.

**Privacy**

Info Tiles requests the phone's approximate location only to retrieve current
weather. Coordinates are sent directly to the Open‑Meteo API and are not sent
to the developer or stored by the watchface. Step and heart-rate values are
read locally from Pebble Health and are not uploaded by the watchface.

**Permissions:** location and health.

## Codex Weekly

**Short description**

Large pixel time, weekly Codex quota and a GitHub-style personal usage heatmap.

**Full description**

Codex Weekly places an oversized digital clock above your remaining weekly
Codex quota and a 12-week personal-usage heatmap. The quota is shown as a
percentage, a segmented progress bar and the time remaining until reset.

Personal data is synced through a private local bridge running on your Mac.
The bridge uses your existing ChatGPT sign-in in Codex; OpenAI credentials are
never stored on the watch. The phone keeps only the bridge URL and a locally
generated bridge token.

Requires Pebble Time 2, a connected Pebble mobile app, a Mac running the
included bridge, and network access between the phone and Mac.

**Privacy**

The bridge requests only Codex rate limits and daily personal-usage counters.
It returns compact watchface data over your own network. The watchface does not
send this data to the developer or any marketplace service. Users should keep
the bridge private or place it behind HTTPS and authentication.

**Permissions:** internet access through PebbleKit JS.

**Website and setup:**
[github.com/meded90/pebble-time-2-pixel-faces](https://github.com/meded90/pebble-time-2-pixel-faces/tree/main/codex-weekly)
