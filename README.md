# TRAILSTRIP

An ESP32-S3 off-road instrument prototype for the Waveshare ESP32-S3-Touch-LCD-3.49 and an iPhone companion app. All data stays on the devices. This is the first bench implementation, not yet vehicle validated.

## Current implementation

- Landscape 640 × 172 LVGL UI: Trail, Camp, Explore; swipe, tap the page title to cycle, or tap a footer dot.
- Automatic normal/upside-down landscape rotation (0°/180° only), with matching touch and sensor-axis mapping. Requires about 600 ms of steady, near-upright orientation; portrait, flat and shock poses retain the last landscape direction. The last orientation is saved in NVS and restored before the first frame at boot; auto-rotation remains active afterward.
- QMI8658 gyro-assisted tilt with acceleration rejection, missing-sensor indication, and persistent zero calibration in NVS.
- Camp bubble level and SET ZERO. In this first version the zero is shared with Trail.
- Explore elevation, speed, pressure, and camp/trip footer, supplied over BLE.
- Missing, invalid, disconnected, or expired telemetry shows dashes; no simulated driving values.
- Native SwiftUI iPhone app: user-started location/barometer session, BLE discovery/reconnection, local camp waypoint and rough trip accumulation.

Trail uses code-native amber Jeep illustrations, angle scales, and large numbers. Tap the Roll Jeep to toggle FRONT / REAR; the rear view has a spare tire and reverses the visual roll direction. The roll number and gauge continue to show the same vehicle measurement. The view choice survives page changes, reboot and power loss. Orientation and view preferences are written only when changed. The Pitch Jeep remains a side view. The 640 × 172 LVGL preview is in `docs/previews/trail-jeep.png` (illustrative 8°/12° input). Illustration rotation is visually capped at ±45° and scale pointers at ±30° to fit the screen; numerical readings retain their full range. Invalid readings dim the vehicles and hide scale pointers. Pitch and roll now use an upright, driver-facing landscape mount: pitch uses negative sensor Z, with negative sensor Y as the upright reference; roll keeps its established direction. Level rest is expected near (0, -1, 0) g. Positive roll denotes vehicle right side down; positive pitch denotes nose up. Verify direction signs on the physical mount. A gravity-vector complementary filter uses the gyroscope for rotation and slow, confidence-gated accelerometer correction to reduce acceleration/braking errors. Gyro integration starts only after a two-second stable acceleration/gyro window estimates its zero-rate bias; until then, readings use smoothed acceleration. Leave the gadget still briefly after startup. A persistent large disagreement falls back to accelerometer readings and reacquires gyro bias rather than drifting indefinitely. Short speed changes are covered by host simulations (strong acceleration is rejected for up to three seconds before recovery); real vehicle-motion validation remains. Sustained acceleration or booting while accelerating can still bias tilt; this is not a fully acceleration-compensated navigation system. SET ZERO only subtracts small angular offsets; it does not solve arbitrary mounting rotation. Calibration is saved under `zero_mount_v2`. Previous `zero_upright` offsets are read with their pitch sign converted, preserving the same physical zero; the earlier flat-board `zero` offsets are ignored. After updating, use Camp → SET ZERO while parked level to compensate for mounting cant.

## Firmware tools

VS Code + PlatformIO Core 6.2.0. Pinned PlatformIO Espressif32 6.13.0 provides ESP-IDF 5.5.3. Managed dependencies: LVGL 8.4.0, AXS15231B 1.0.1~1, and TCA9554 2.0.3. `dependencies.lock` records transitive versions. Source layout also uses standard ESP-IDF CMake.

```sh
pio run -e trailstrip_v2
pio run -e trailstrip_v1
pio device list
```

On this Mac, PlatformIO is installed at `~/.platformio/penv/bin/pio` but is not currently on PATH. The pre-existing esptool installation required `intelhex==2.3.0` in its Python environment; this dependency was installed during setup.

## Identify the board before uploading

- **V2 / PCB Rev1.1:** `trailstrip_v2`, backlight GPIO42 and expander-driven display reset.
- **V1:** `trailstrip_v1`, backlight GPIO8 and display reset GPIO21.

The environment defaults to V2 for building only; do not assume a connected board is V2. The user confirmed this device is V2; use `trailstrip_v2` when uploading to it. Confirm the marking separately for any other board. Both configurations use 16 MB flash and octal PSRAM according to the vendor hardware configuration.

Once a real serial port appears, inspect chip and flash size, preserve existing firmware if needed, then upload the matching environment:

```sh
pio run -e trailstrip_v2 -t upload --upload-port /dev/cu.usbmodemYOUR_PORT
pio device monitor --port /dev/cu.usbmodemYOUR_PORT --baud 115200
```

Uploading replaces the application. The confirmed V2 board was successfully flashed on 2026-11-13 after saving a full original-flash backup in `backups/`. Boot logs confirm display/IMU initialization and BLE advertising. No full-chip erase or fuse programming was performed. Visual/touch checks and iPhone telemetry remain to be verified. Native USB Serial/JTAG logging is enabled. Expected startup messages include `TRAILSTRIP 0.1.0`, `QMI8658 ready`, and `UI ready 640x172`.

If no USB device appears: use a known data cable connected directly to the Mac, then hold BOOT, press/release RESET, and release BOOT. Check `pio device list` again.

## iPhone app

Open `ios/Trailstrip.xcodeproj` in Xcode. Select your signing team and a unique bundle identifier, select your physical iPhone, and run. This project has no team or credentials embedded. Accept Bluetooth, Location, and Motion permissions, then tap **Start session**. It connects to the first advertising Trailstrip device.

The unsigned iPhoneOS build can be checked without installing:

```sh
xcodebuild -project ios/Trailstrip.xcodeproj -scheme Trailstrip \
  -sdk iphoneos -configuration Debug -derivedDataPath /tmp/trailstrip-ios-build \
  CODE_SIGNING_ALLOWED=NO build
```

Background location and Bluetooth modes are declared, but locked-screen location, barometer delivery, reconnect behavior, and battery use have not been tested. Barometer data expires independently, including when iOS stops delivering it. No automatic restart after force quit is implemented. The prototype accepts one BLE connection without pairing/bonding; access control remains a later milestone.

## Validation

```sh
clang++ -std=c++17 -DTRAILSTRIP_HOST_TEST -Imain \
  tests/telemetry_test.cpp main/telemetry.cpp -o /tmp/trailstrip-telemetry-test
/tmp/trailstrip-telemetry-test
```

These tests cover packet validation, units/ranges, invalid values, and expiry. See `docs/VALIDATION.md` for the actual build and hardware status, and `docs/BLE_PROTOCOL.md` for interoperability.

## Rendering the actual firmware UI on a Mac

The headless preview compiles the same `main/ui.cpp` and `main/vehicle.cpp` against LVGL, with desktop-only sensor stubs. This avoids treating a concept rendering as proof of the implemented layout.

```sh
cmake -S scripts/ui_preview -B /tmp/trailstrip-ui-preview
cmake --build /tmp/trailstrip-ui-preview -j 8
/tmp/trailstrip-ui-preview/preview /tmp/trail.ppm 8 12
sips -s format png /tmp/trail.ppm --out /tmp/trail.png
```

Preview arguments: output PPM path, roll, pitch, page (0–2), valid (0/1), optional Roll tap count (simulated LVGL pointer input), inverted landscape (0/1), and saved rear view (0/1). Requires the managed LVGL dependency downloaded by a PlatformIO build.

Upright orientation regression check:

```sh
clang++ -std=c++17 -Imain tests/attitude_test.cpp -o /tmp/trailstrip-attitude-test
/tmp/trailstrip-attitude-test
```

Landscape auto-rotation regression check (debounce, ambiguous poses, sample gaps, complete framebuffer/touch mapping, and equivalent sensor mounts):

```sh
clang++ -std=c++17 -Imain tests/landscape_test.cpp -o /tmp/trailstrip-landscape-test
/tmp/trailstrip-landscape-test
```

Display preference persistence regression check (real preference code with a fake NVS backend):

```sh
clang++ -std=c++17 -Itests/stubs -Imain tests/display_preferences_test.cpp main/display_preferences.cpp -o /tmp/trailstrip-preferences-test
/tmp/trailstrip-preferences-test
```

### Phone-synced clock

The fourth **CLOCK** screen appears after the iPhone app connects and sends time. Start a session in the updated app, then swipe left from Explore or tap the fourth dot. Large 24-hour digits show hours, minutes and seconds; the footer shows the phone-local date. GPS permission/fix is not needed for time sync. The screen disappears on disconnect or after 60 seconds without a time update, returning to Explore.

Clock parser and freshness checks:

```sh
clang++ -std=c++17 -DTRAILSTRIP_HOST_TEST -Imain tests/clock_test.cpp main/telemetry.cpp -o /tmp/trailstrip-clock-test
/tmp/trailstrip-clock-test
```

The preview also accepts `clock_sync` and `disconnect_after_render` (0/1) after `saved_rear`. Example clock rendering:

```sh
/tmp/trailstrip-ui-preview/preview /tmp/clock.ppm 0 0 3 1 0 0 0 1
```

Explore readings use 48 px digits (with a 32 px fallback for unusually long values). To render sample readings in the desktop preview, set `TRAILSTRIP_PREVIEW_EXPLORE=1284,8.5,1013.2` (elevation m, speed km/h, pressure hPa) and select page 2.

Camp → **SET ZERO** opens a confirmation prompt. Park level and hold still, then tap **CONFIRM** to replace the saved calibration or **CANCEL** to keep it. Preview the prompt with `TRAILSTRIP_PREVIEW_ZERO=prompt` on page 1; `cancel` and `confirm` exercise the touch flow and calibration-call assertions.

### Clock city and weather

CLOCK now shows temperature (°C) and large white sunrise and sunset times on the left; the large clock on the right; today's forecast, high/low and precipitation chance above the digits; and date/city in the footer. City replaces PHONE LOCAL TIME. Start a session in the updated iPhone app with Location permission and internet access. No API key is needed for this personal prototype. Weather is supplied by [Open-Meteo](https://open-meteo.com/) with attribution in the app; city lookup uses Apple.

Forecast refreshes about every 15 minutes and after moving 5 km, with retries no more than once a minute. Cached weather expires after 30 minutes; old dates, missing sun events and unavailable weather display dashes. The clock itself remains available as long as phone time sync is fresh. City names are transliterated/truncated to 16 characters on the gadget. Sunrise/sunset use the same phone timezone as the clock.

Weather tests:

```sh
xcrun swiftc -module-cache-path /private/tmp/trailstrip-swift-module-cache ios/Trailstrip/WeatherFeed.swift tests/weather_test.swift -o /private/tmp/weather-swift-test
/private/tmp/weather-swift-test
clang++ -std=c++17 -DTRAILSTRIP_HOST_TEST -Imain tests/weather_test.cpp main/telemetry.cpp -o /private/tmp/weather-test
/private/tmp/weather-test /private/tmp/trailstrip-swift-weather.bin
```

Set `TRAILSTRIP_PREVIEW_WEATHER=normal` (or `long`, `stale`, `polar`) with the clock preview arguments to inspect fixture data. Preview values are illustrative, not live observations.

### BLE reconnection recovery

The iPhone first checks for a Trailstrip connection already held by iOS, then scans. Connection, discovery, and write stalls trigger cancellation/retry; if cancellation never completes, the app recreates its CoreBluetooth manager. Callbacks from obsolete connections/managers are ignored. Stop/start waits for cancellation or its bounded recovery instead of keeping a stale peripheral indefinitely.

ESP32 advertising runs from a deferred NimBLE host callback, with automatic retries. A link receiving no valid telemetry for 120 seconds is disconnected to free the single connection slot. Failed termination or repeated advertising failures trigger a BLE-host reset without restarting the display or changing calibration/preferences. A background-suspended phone may therefore be disconnected after two minutes and must reconnect when it resumes.

Host recovery checks:

```sh
clang++ -std=c++17 -Imain tests/ble_recovery_test.cpp -o /private/tmp/ble-recovery-test
/private/tmp/ble-recovery-test
xcrun swiftc -module-cache-path /private/tmp/trailstrip-swift-module-cache ios/Trailstrip/BLELinkState.swift tests/ble_link_test.swift -o /private/tmp/ble-link-test
/private/tmp/ble-link-test
```

For diagnosing a recurrence, the USB log now records connection handles, disconnect reasons, advertising failures/retries, idle termination and BLE host resets.

### Camp level-assist layout

Camp shows a concentric bubble target, large signed roll/pitch readings, and a top-down Jeep with its nose pointing right (`FRONT >`). Amber wheels identify the low side/corner relative to saved calibration: positive roll means left low (corrected from mounted-device feedback); positive pitch means rear low. Within ±0.5° on both axes it shows NEAR LEVEL. Invalid IMU readings dim the Jeep and show HOLD STILL. These are attitude cues, not measured wheel heights or ramp-height instructions. SET ZERO retains its Cancel/Confirm prompt.

The bubble keeps its established axis directions and stays inside the target at extreme compound angles. Preview: `docs/previews/camp-level-assist.png`. Sign/deadband regression check: compile `tests/camp_level_test.cpp` with `clang++ -std=c++17 -Imain` and run the resulting executable.

### Direct Veepeak OBD connection / Vehicle screen

The ESP32 now connects directly to a Veepeak OBDCheck BLE while retaining the iPhone link. VEHICLE is the fifth page and works independently of the phone; CLOCK is skipped when unavailable. It shows supported coolant temperature, RPM and speed plus adapter-measured voltage. Unsupported/stale readings show dashes. Tire pressures and transmission temperature are not implemented.

See [OBD setup, protocol and test status](docs/OBD_CONNECTION.md). The scanner was unavailable during development, so in-vehicle validation is pending. Preview with illustrative readings: `docs/previews/vehicle-obd.png`.

### City arrival popup

A newly reported city different from the last known city shows `Current location` and a large centered city name for five seconds on the top overlay layer over any page. The first fresh city after boot also shows the notice. Repeated packets and reconnecting to the same city do not repeat it. A new city during an existing notice replaces it and restarts the five-second duration. Set Zero confirmation takes priority, with the latest fresh city notice deferred until it closes. Touches do not pass through the popup.

This uses the existing phone city lookup cadence (normally 15 minutes or 5 km movement), not live administrative-boundary detection. The existing 16-character BLE city-name limit still applies. Preview: `docs/previews/city-popup.png`. Host checks: `tests/city_notice_test.cpp`; actual LVGL timing checks via `TRAILSTRIP_PREVIEW_CITY=expire` (combine with `TRAILSTRIP_PREVIEW_ZERO=prompt` on Camp to check deferred display).
