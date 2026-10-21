# Contributing

Thanks for picking this up. Three independent pieces, each with its own loop.

## Never commit credentials

`firmware/include/secrets.h` is gitignored and is the **only** place real WiFi
and token values belong. `config.h` picks it up via `__has_include` and falls
back to placeholders, so the repo builds without it.

```bash
cp firmware/include/secrets.example.h firmware/include/secrets.h
```

If you've already put a real SSID or token in a tracked file, don't just delete
the line — it stays in history. Say so in the PR and we'll rotate.

## The simulator is the fast loop

Layout on a round panel is easy to get wrong, and flashing to check is slow.
The renderer builds and runs on your machine using the *same* `dotgrid.cpp`,
`cards.cpp` and `fonts.cpp` as the firmware, with a stub framebuffer:

```bash
bash tools/sim/build.sh
./tools/sim/sim /tmp/out && python3 tools/sim/png.py /tmp/out
open /tmp/out
```

It exits non-zero if any glyph lands where the circle has no dot — which is
exactly what a too-long label looks like before you've flashed anything. CI
runs this on every PR, so a layout regression fails the build.

**Any PR that changes something visual should include a before/after PNG from
the simulator.** It takes ten seconds and makes review actually possible.

## Firmware

```bash
cd firmware
pio run                 # build
pio run -t upload       # flash
pio device monitor      # fps / heap heartbeat every 5s
```

`DEMO_MODE 1` in `config.h` (the default) runs with synthetic stats and fake
sales — no WiFi, no Stripe. Leave it on unless you're specifically working on
the network path.

Note the platform is the **pioarduino** fork, not `espressif32`. Arduino_GFX
1.5 needs ESP32 Arduino core 3.x and the official PlatformIO platform is still
on 2.0.17.

## Relay

```bash
cd relay
npm install
npx tsc --noEmit
npx wrangler deploy --dry-run --outdir /tmp/w
npx wrangler dev        # local, against your own secrets in .dev.vars
```

`.dev.vars` is gitignored. It wants `STRIPE_SECRET_KEY`,
`STRIPE_WEBHOOK_SECRET` and `DEVICE_TOKEN`.

## Adding a card

1. An enum in `firmware/include/cards.h`
2. A `case` in `drawCard()` in `firmware/src/cards.cpp`
3. If it needs new data, a field on `Snapshot` in `relay/src/stripe.ts` and the
   matching parse in `firmware/src/relay.cpp`
4. Add it to the sim's loop so it gets rendered and clip-checked
5. Add it to `IDLE_CARDS` in `config.h`

Keep labels short. At the default 8px pitch the usable width at a label row is
about 11 characters, and the sim will fail the build if you exceed it.

## Style

Match what's there: two-space indent, comments that explain *why* rather than
restate the code. Don't reformat files you're not otherwise changing.
