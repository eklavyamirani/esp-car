# CI Pipeline Strategy

This document lays out a CI strategy for the esp-car firmware, phased so that each
stage delivers value on its own. The guiding constraint is that this is an embedded
project: the highest-value checks (does it drive?) need real hardware, so the pipeline
is split into what can run on a cloud runner (build, lint, host-side unit tests) and
what needs the physical car (hardware-in-the-loop).

## Goals

1. **Never merge a build break.** Every PR must compile against the pinned ESP-IDF
   version before it can merge.
2. **Catch logic bugs without hardware.** Driver/mapping logic (PCA9685 register math,
   motor channel mapping, speed clamping) should be unit-testable on a Linux host.
3. **Keep hardware-in-the-loop testing available but out of the merge path.** The
   existing `test.sh` flow stays the gold standard, run on demand or nightly via a
   self-hosted runner — never as a required PR check.
4. **Detect upstream drift early.** A scheduled build against newer ESP-IDF releases
   surfaces breakage before an intentional upgrade.

## Platform

**GitHub Actions**, since the repo is hosted on GitHub. Espressif publishes official
Docker images (`espressif/idf:<version>`) with the full toolchain preinstalled, which
makes the build job trivial and fast — no toolchain download per run.

## Pipeline stages

### Stage 1 — Build verification (implement first)

The core job. Runs on every PR and every push to `main`:

- Container: `espressif/idf:v5.4` (pin the exact tag; upgrading IDF becomes an
  explicit, reviewable diff).
- Command: `idf.py build` in `firmware/` with `IDF_TARGET=esp32`.
- Enable `ccache` and cache it via `actions/cache` keyed on the IDF version — cuts
  warm builds from minutes to seconds.
- Upload `esp-car.bin` / `.elf` / `.map` as workflow artifacts (short retention,
  ~7 days) so any green commit can be flashed without a local toolchain.
- Run `idf.py size-components` and print it in the job summary. Optionally enforce a
  flash/RAM budget later — on a 4 MB part this becomes relevant once the camera
  milestone lands.

Workflow hygiene:

- `concurrency` group per branch with `cancel-in-progress: true` so stale PR pushes
  don't queue.
- Path filter to `firmware/**` and the workflow file itself, so docs-only PRs skip
  the build.
- Make this job a **required status check** on `main` via branch protection.

Example workflow (`.github/workflows/build.yml`):

```yaml
name: build
on:
  push:
    branches: [main]
    paths: ['firmware/**', '.github/workflows/build.yml']
  pull_request:
    paths: ['firmware/**', '.github/workflows/build.yml']

concurrency:
  group: build-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build:
    runs-on: ubuntu-latest
    container: espressif/idf:v5.4
    steps:
      - uses: actions/checkout@v4
      - uses: actions/cache@v4
        with:
          path: ~/.ccache
          key: ccache-idf5.4-${{ github.sha }}
          restore-keys: ccache-idf5.4-
      - name: Build firmware
        run: |
          . $IDF_PATH/export.sh
          cd firmware
          idf.py build
          idf.py size-components >> $GITHUB_STEP_SUMMARY
      - uses: actions/upload-artifact@v4
        with:
          name: firmware
          path: |
            firmware/build/esp-car.bin
            firmware/build/esp-car.elf
            firmware/build/*.map
          retention-days: 7
```

### Stage 2 — Static checks (cheap, add alongside Stage 1)

A separate fast job, not gated on the toolchain image:

- **clang-format** check over `firmware/components/**` and `firmware/main/**`
  (add a `.clang-format`; enforce format-only diffs stay out of reviews).
- **cppcheck** for C static analysis (catches null derefs, buffer issues in the
  drivers). ESP-IDF also ships `idf.py clang-check` if deeper analysis is wanted
  later.
- **shellcheck** for `flash.sh` / `test.sh`.
- **ruff** for `tools/serial_check.py` and future Python tooling.

All advisory at first (non-required), promoted to required once the tree is clean.

### Stage 3 — Host-side unit tests (highest engineering value)

ESP-IDF v5.x supports building for the **`linux` target**, which lets Unity-based
tests run directly on the CI runner — no device, no QEMU. The plan:

- Restructure component logic so pure computation is separable from I2C transport:
  the PCA9685 register/ON-OFF-count math and the motor sign/clamp/channel-mapping
  logic take no hardware dependency.
- Add a `test_apps/` host test app with Unity tests for:
  - PWM value → register byte encoding, frequency → prescale math (`pca9685`).
  - Speed sign → IN1/IN2 channel selection, clamping at ±4095, direction
    multipliers (`motor`).
  - I2C calls verified against a mock/fake `i2c` layer (ESP-IDF provides Linux
    mocks for common drivers, or a thin function-pointer seam works).
- CI job: same `espressif/idf` container, `idf.py --preview set-target linux && idf.py build && ./build/test_app.elf`.

This is where regressions in milestones 2–6 (servo math, sensor parsing, command
protocol) get caught without touching the car. New components should land with host
tests from day one — sensor parsing and the TCP command protocol (milestone 4) are
almost entirely host-testable.

### Stage 4 — Hardware-in-the-loop (optional, out of merge path)

The existing `test.sh` (build → flash → assert serial log sequence) is already a
complete HIL test; CI just needs a machine with the car plugged in:

- Register a **self-hosted runner** (e.g., a Raspberry Pi) with the ESP32 on USB and
  labels like `[self-hosted, esp-car]`.
- Trigger via `workflow_dispatch` and/or nightly `schedule`, plus optionally a
  `hitl` PR label. **Never a required check** — a dead battery must not block merges.
- The job runs `firmware/test.sh` verbatim and uploads the captured serial log as an
  artifact on failure.
- Security note: self-hosted runners must not run untrusted fork PRs; restrict the
  HIL workflow to `workflow_dispatch`/`schedule`/same-repo branches.

QEMU (`idf.py qemu`) was considered as a hardware-free middle ground, but it does not
emulate the PCA9685 on the I2C bus, so `motor_init()` fails at boot and the demo
sequence never runs. It's only worth adding later as a boot smoke test ("app starts,
init failure is handled gracefully") — low priority.

### Stage 5 — Upstream drift & releases

- **Nightly/weekly scheduled job**: build against `espressif/idf:latest` (and the
  next release candidate when relevant). Failures open/ping an issue rather than
  blocking anyone — this is an early-warning radar for the eventual IDF upgrade.
- **Release workflow**: on a version tag (`v*`), build, produce a single flashable
  image with `esptool.py merge_bin`, and attach `.bin` + `.elf` to a GitHub Release.
  Useful once the car is usable by people without the toolchain.
- **Dependabot** for GitHub Actions versions (`package-ecosystem: github-actions`).

## Rollout order

| Phase | What lands | Effort | Gate status |
|-------|-----------|--------|-------------|
| 1 | Build workflow + artifacts + branch protection | ~1 hour | Required |
| 2 | clang-format, cppcheck, shellcheck, ruff | ~1–2 hours | Advisory → required |
| 3 | Linux-target Unity tests for `pca9685` + `motor` | ~1 day (includes refactor for testability) | Required once stable |
| 4 | Self-hosted HIL runner running `test.sh` | hardware-dependent | Never required |
| 5 | Nightly IDF-latest build, release workflow, Dependabot | ~1 hour | Informational |

Phase 1 alone eliminates the biggest current risk (unbuildable `main`) and is the
prerequisite for everything else.
