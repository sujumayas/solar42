# Solar 42N — release, signing & notarization notes (M9c P6)

Status: **notes only** — nothing is distributed yet. Two blockers before any
public artifact, both tracked in `08-implementation-plan.md`:

1. **Branding pass** (plan risk #5): replace the Elta wordmarks / trade
   dress (SOLAR 42ᴺ lockup, СОЛАР 42N, cartridge names) with original
   branding. Layout and behavior are fair game; names and logos are not.
2. **User listening sign-off** on the milestone tree (the ear is the gate).

## Release gate (already automated)

`app/scripts/check.sh` **is** the release gate — a candidate build must be
cut from a commit where it runs ALL GREEN:

- configure + build (RelWithDebInfo) + ctest (unit, routing, FV-1 goldens,
  RT malloc guard, statecheck)
- pluginval **strictness 10** on the VST3 (`PLUGINVAL_STRICTNESS` to override)
- **auval** on the AU (`aumu S42n S42p`, auto-installed to the user domain)
- `solar42n_render --bench 10` — CPU under 10 % of one core @ 48 k / 128
- render smoke (engine produces audio)

## Identities & prerequisites

- Apple Developer Program membership (Team ID `TEAMID` below).
- Two Developer ID certificates in the login keychain:
  - `Developer ID Application: <Name> (TEAMID)` — signs the .app/.component/.vst3
  - `Developer ID Installer: <Name> (TEAMID)` — only if shipping a .pkg
- A notarytool keychain profile (once):
  `xcrun notarytool store-credentials s42n-notary --apple-id <id> --team-id TEAMID --password <app-specific-pw>`

## Signing (per artifact)

Bundles to sign after a clean `check.sh` build
(`app/build/Solar42N_artefacts/RelWithDebInfo/`):

- `Standalone/Solar42N.app`
- `AU/Solar42N.component`
- `VST3/Solar42N.vst3`

For each, **hardened runtime + secure timestamp**, deep-signing is not
needed (JUCE bundles carry no nested frameworks; if that changes, sign
inner binaries first, outermost last):

```sh
codesign --force --options runtime --timestamp \
         --sign "Developer ID Application: <Name> (TEAMID)" \
         Solar42N.app        # repeat for .component and .vst3
codesign --verify --deep --strict --verbose=2 Solar42N.app
```

Notes:
- The dev build is ad-hoc linker-signed (`Signature=adhoc`); Developer ID
  re-signing replaces it (`--force` required).
- No JUCE entitlements are needed beyond hardened runtime for this plugin
  set (no mic entitlement: the standalone requests input via the OS
  permission prompt — keep `NSMicrophoneUsageDescription` in the plists,
  JUCE generates it).

## Notarization + stapling

Notarize a zip per artifact (or one zip holding all three), then staple
**each bundle**:

```sh
ditto -c -k --keepParent Solar42N.app Solar42N-app.zip
xcrun notarytool submit Solar42N-app.zip --keychain-profile s42n-notary --wait
xcrun stapler staple Solar42N.app     # repeat for .component / .vst3
spctl --assess -vv --type execute Solar42N.app   # Gatekeeper check ("accepted")
```

Plugins (.component/.vst3) can't be `spctl --type execute` checked; verify
with `stapler validate` instead.

## Packaging / install paths

Simplest acceptable distribution: a notarized zip per format with install
instructions. If a .pkg is wanted later, `pkgbuild --component` per bundle
+ `productbuild`, signed with the Installer identity, then notarize the pkg.

- AU → `~/Library/Audio/Plug-Ins/Components/` (user) or `/Library/...` (system)
- VST3 → `~/Library/Audio/Plug-Ins/VST3/`
- Standalone → `/Applications`
- After AU install: `killall -9 AudioComponentRegistrar` (or reboot) if a
  DAW's AU list is stale; `auval -a | grep S42n` to confirm registration.

## Version stamping

`CMakeLists.txt` `VERSION` (currently 0.1.0) drives the bundle versions —
bump per release; DAW state carries its own `stateVersion` (currently 1,
migration-tolerant loader), so plugin version and state version move
independently.

## Windows/Linux

Out of scope until the plan's backlog item lands (no cross-platform CI
yet). VST3 signing on Windows would use a standard Authenticode cert;
Linux ships unsigned tarballs by convention.
