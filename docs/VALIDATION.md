# Gyro runaway correction — 2026-11-16

- User reported continuously increasing roll/pitch after gyro fusion. The previous learner required raw rates below 0.5 dps, so larger sensor offsets were never learned; once disagreement exceeded 10 degrees, gravity correction was locked out indefinitely. Prior host coverage only tested a 0.2 dps offset and missed this failure.
- Gyro integration now waits for two seconds of stable acceleration direction and gyro samples, estimating bias without requiring a near-zero raw rate. Until calibration, readings use smoothed acceleration. Persistent large disagreement for three seconds switches back to acceleration and restarts bias acquisition. Saved user zero offsets are unchanged.
- Stable windows cannot prove rest under constant yaw or sustained acceleration; startup while moving may defer calibration or bias its estimate. Strong acceleration rejection is now bounded by the three-second recovery path. This trades some sustained-acceleration rejection for recovery from drift.
- Host regression passed for the previously observed (-8.078, -9.836, -3.695) dps rates simulated as a stationary offset over two minutes, including the observed acceleration magnitude. Also passed unexpected post-calibration bias recovery, moving-start calibration deferral, two-second strong acceleration/braking, five-second moderate acceleration, real/compound rotation, inversion, shocks and gaps. Existing attitude, landscape and Camp guidance checks passed.
- V2 build/upload passed with verified hashes. A 32-second hardware capture initially showed changing pose while waiting for stable bias, then acquired bias (0.885, -0.355, -0.325) dps at 13.9 seconds. Subsequent samples at 17.1/22.2/27.3 seconds showed roll -2.53/-2.52/-2.55 degrees and pitch -32.99/-32.95/-33.01 degrees: ranges 0.03/0.06 degrees, without continued runaway. Values are raw mounted angles before saved user offsets. No panic observed. Driving behavior remains unverified.
- Logs: `logs/upload-drift-fix-2026-11-16.log`, `logs/boot-drift-fix-2026-11-16.log`.

# Gyro-assisted pitch / acceleration rejection — 2026-11-16

- Replaced accelerometer-only angle smoothing with a sensor-frame gravity-vector complementary filter. Enabled QMI8658 gyro at ±256 dps / 112.1 Hz; updates use measured UI sample intervals and exact vector rotation. Existing mounting axes, zero offsets and front/rear conventions are preserved.
- Accelerometer correction is slow (4 seconds near agreement, 20 seconds for larger disagreement), rejected above about 10 degrees disagreement or 0.04 g norm deviation. Stable low-rate periods learn gyro bias. Missing samples invalidate the display after 500 ms; gaps above 250 ms reacquire rather than integrating stale rates. Auto-rotation uses estimated gravity.
- Host tests passed: five-second ±0.3 g acceleration/braking, ±0.1 g with less than 1.5 degrees error, actual pitch/roll rotation in both mounting orientations, simultaneous acceleration and pitch rotation, coupled tilt, physical 180-degree flip, shock/free fall, invalid gyro, gaps and stationary bias. Existing attitude, landscape and Camp guidance tests passed.
- V2 build/upload succeeded with flash hash verification on `/dev/cu.usbmodem101`. Ten-second boot capture confirmed gyro-assisted IMU initialization, valid fused sample, UI startup and phone connection without panic. The first raw gyro sample was nonzero; physical motion/bias was not independently measured.
- Real acceleration/braking and gyro direction on the mounted device still require driving/physical validation. Sustained acceleration, motion during initialization, missed sampling during UI stalls and bias drift remain limitations; no GPS/OBD acceleration compensation is implemented.
- Logs: `logs/upload-fusion-2026-11-16.log`, `logs/boot-fusion-2026-11-16.log`.

# City popup missed-notification correction — 2026-11-16

- User reported the city changing in the footer without a popup. The exact original failure was not reproduced. Found a lost-notice edge case: a deferred city was acknowledged before display, so a temporary invalid city packet could discard it permanently.
- Tracker now acknowledges only when a popup is requested; fresh city data retries a deferred change after invalidation. First fresh city after boot also shows a notice. Repeated/same-city reconnect packets remain suppressed.
- Moved the popup to LVGL's top overlay layer, explicitly updated its layout/invalidation, and start the five-second deadline after creation. Added logs for received city changes and popup show/close events. Read the clock after taking the telemetry snapshot to avoid a receipt-time race.
- Host regression tests passed for deferred invalidation/retry, first city, duplicates, reconnect, latest pending city and returning to the already-shown city. Actual LVGL expiry, inverted orientation and deferred Set Zero tests passed; preview visually checked. Weather parser regression passed.
- V2 build/upload succeeded with flash hash verification on the gadget's new port `/dev/cu.usbmodem101`. User confirmed: “Yes, the popup appears” on the flashed hardware.
- Hardware log confirmed city receipt, popup display, and dismissal approximately five seconds later; no panic observed.
- Logs: `logs/upload-city-fix-2026-11-16.log`, `logs/boot-city-fix-2026-11-16.log`.

# Five-second city arrival notice — 2026-11-16

- Added a centered overlay across all pages: Entering city of / city name in 48 px text (32 px fallback for unusually wide names). Automatically closes after five monotonic seconds, returning to the underlying page. New changes replace the notice and restart its duration.
- First fresh city establishes a baseline; repeats, temporary invalid data and reconnecting to the same city do not retrigger it. Last city retained in RAM only. Set Zero confirmation has priority and defers the latest fresh notice. Underlying gestures/touches are blocked during the notice, with release required across appearance/expiry.
- Host city-change tests passed for baseline/repeats, reconnect, pending replacement, invalid-data cancellation and subsequent changes. Actual LVGL tests verified 4.999-second visibility, five-second expiry, no repeat, inverted landscape, deferred display after cancelling Set Zero and the existing confirm flow. Preview visually inspected: `docs/previews/city-popup.png`.
- Uses current phone lookup cadence and 16-character city transport; no real-time boundary detection or iOS update.
- V2 build/upload passed with verified hashes. Eight-second boot capture showed UI/IMU ready and phone connected without panic. Real travel-triggered notification remains for user observation.
- Logs: `logs/upload-city-popup-2026-11-16.log`, `logs/boot-city-popup-2026-11-16.log`.

# Direct Veepeak BLE client / Vehicle screen — 2026-11-15

- Enabled NimBLE central+observer roles and two connections while retaining the existing phone peripheral/server role. Added scanner-only GAP/GATT callbacks, generation guards for stale callbacks, bounded discovery/subscription/ELM transactions, reconnect backoff and data clearing. Normal scanner errors do not terminate the phone link; stuck termination can reset the shared host.
- Scanner discovery targets VEEPEAK names and verifies FFF0/FFF1 notify/FFF2 write plus CCCD. Profile is based on documented first-hand observation and requires verification on the user's adapter. No arbitrary-characteristic guessing.
- Added serialized, bounded ELM327 initialization and standard supported-PID polling for coolant/RPM/speed, plus adapter voltage. Read-only command set; missing/unsupported/stale readings show dashes. Vehicle is a stable fifth page accessible without phone time; Clock is skipped when unavailable.
- Production ELM engine tests passed: per-byte notification fragmentation, echo/SEARCHING text, capability bitmaps, unit conversions, unsupported-PID skipping, NO DATA, wrong PID/header/truncated frames, invalid voltage, delayed capability retry, command timeout, overflow, non-ASCII input and reconnect reset. Existing Set Zero confirmation and Clock-disconnect preview checks passed.
- Actual 640 × 172 LVGL Vehicle layout preview checked with illustrative live and stale/offline inputs. Artifact: `docs/previews/vehicle-obd.png`.
- V2 build/upload succeeded with verified flash hashes. Firmware SHA-256 `400eda55d7b133d55795249c7f161b75569dcfb5bc323e1eddfed62adec38fb3`. Twenty-five-second boot capture showed UI/IMU ready, phone connection handle 2, and repeated scanner discovery while the phone remained connected; no panic observed.
- User stated the scanner was unavailable. Actual scanner GATT discovery, Jeep PID support, dual active-link stability and scanner/ignition reconnect behavior are NOT hardware-validated. Instructions and protocol limits: `docs/OBD_CONNECTION.md`.
- Logs: `logs/boot-obd-2026-11-15.log`, `logs/upload-obd-2026-11-15.log`.

# Camp left/right guidance correction — 2026-11-15

- Reversed only the lateral low-side mapping following mounted-device feedback. Positive roll now selects left-low; negative selects right-low. Matching front/rear corner labels and highlighted wheels follow this correction. Numerical roll, bubble movement, pitch/front-rear mapping and calibration are unchanged.
- Direction/deadband/invalid-state tests passed; added explicit label assertions. Actual LVGL preview at +1.2°/-0.6° shows FRONT LEFT LOW and the corresponding wheel highlight (`docs/previews/camp-level-assist.png`).
- V2 build/upload passed with verified flash hashes. Eight-second startup capture showed UI/IMU ready and phone connected without panic.
- Logs: `logs/upload-camp-side-2026-11-15.log`, `logs/boot-camp-side-2026-11-15.log`.

# Camp concept implementation — 2026-11-15

- Reworked Camp into concentric target/crosshairs, two large signed values with labels below, and top-down Jeep guidance. Jeep nose points right and is marked FRONT >. Low wheels are highlighted in amber, with side/corner text; both axes within ±0.5° display NEAR LEVEL. Guidance is relative to saved calibration, not actual wheel-height measurement.
- Preserved established bubble directions; radial clamping keeps the bubble within the circle at compound/extreme tilt. Invalid samples center/dim the bubble, dim the Jeep and display HOLD STILL without low-wheel highlights.
- Retained SET ZERO confirmation, using an outlined button below the Jeep. Actual LVGL touch simulations passed cancel, confirm, invalid-reading failure, and inverted-landscape confirmation.
- Pure direction tests passed for all four low sides, four low corners, ±0.5° deadband and invalid/nonfinite inputs. Actual LVGL previews visually checked at +1.2°/-0.6°, -179.9°/+89.9°, and missing data. Artifact: `docs/previews/camp-level-assist.png` (sample inputs).
- V2 build/upload passed with flash hash verification. Eight-second boot capture showed IMU/UI ready and phone connected without panic. Physical low-corner response still requires user observation on the mounted gadget.
- Logs: `logs/upload-camp-redesign-2026-11-15.log`, `logs/boot-camp-redesign-2026-11-15.log`.

# BLE reconnect recovery — 2026-11-15

- User reported reconnect failures requiring ESP32 power cycling. Initial USB capture showed a successful reconnect, so the original failure was not reproduced in that capture. Code inspection found no advertising retry after failure, no stale-link cleanup, and scan-only iOS reconnect with no deadlines for connection/discovery/writes/cancellation.
- ESP32 now defers advertising onto the NimBLE host queue, retries errors every second, checks scan-response errors, tracks/reset-clears connection state, and logs GAP reasons. No valid packets for 120 seconds triggers disconnect; a stuck termination (10 seconds) or 10 advertising/termination failures triggers BLE-host reset without rebooting the gadget.
- iOS retrieves system-connected peripherals before scanning, uses monotonic connect/discovery/write deadlines (15/10/8 seconds), and recreates its central manager after an 8-second cancellation timeout. Old-manager/peripheral callbacks are filtered; failed writes, missing/changed services and lost links enter recovery. Stop/start handles outstanding cancellation.
- Host policy/deadline tests passed, including healthy traffic, advertising failures, idle connections, stuck termination/cancellation, and 50 simulated lifecycle cycles per side. These are host simulations, not 50 physical phone reconnections.
- Signed iOS build and installation passed. Automatic launch was denied because the iPhone was locked; the user must unlock/open the app. V2 firmware build/upload passed with verified hashes; SHA-256 `667cd6f2f254c1989598e2426070a3967f2eda4aacb90946e65ceee7f6304898`. Fifteen-second boot log showed BLE host sync, advertising started, and IMU/UI ready without panic.
- Repeated physical iPhone stop/start, force quit/relaunch, out-of-range and Bluetooth-off/on tests remain for user confirmation. Diagnostic logging supports follow-up if the intermittent failure recurs.
- Logs: `logs/ble-before-fix-2026-11-15.log`, `logs/boot-ble-recovery-2026-11-15.log`, `logs/upload-ble-recovery-2026-11-15.log`, and `logs/ios-ble-recovery-{build,install}-2026-11-15.log`.
- References: [Apple connected peripheral retrieval](https://developer.apple.com/documentation/corebluetooth/cbcentralmanager/retrieveconnectedperipherals(withservices:)), [Apple asynchronous cancellation](https://developer.apple.com/documentation/corebluetooth/cbcentralmanager/cancelperipheralconnection(_:)), [NimBLE GAP lifecycle](https://mynewt.apache.org/v1_8_0/network/ble_hs/ble_gap.html). Used the pinned local ESP-IDF/NimBLE headers to verify APIs.

# Larger Camp readings — 2026-11-15

- Increased roll/pitch values from 32 px to 48 px, keeping signs and one decimal place. Values wider than 170 px fall back to 32 px to avoid the adjacent reading or Set Zero button.
- Actual LVGL previews visually checked with +1.2° / -0.6° and -123.4° / +89.9°. Preview: `docs/previews/camp-large.png`.
- V2 build/upload passed with verified flash hashes. Eight-second startup capture showed IMU/UI ready without panic.
- Logs: `logs/upload-camp-large-2026-11-15.log`, `logs/boot-camp-large-2026-11-15.log`.

# Clock city, sun times and forecast — 2026-11-14

- Redesigned Clock with temperature/sunrise/sunset left, large clock right, daily condition/high/low/precipitation chance above digits, and date/city footer. Existing connected/time-sync gating and landscape rotation remain.
- iPhone uses Apple reverse geocoding and Open-Meteo HTTPS forecast API. Forecast refresh is 15 minutes or 5 km movement; requests/retries are throttled to at least 60 seconds. Memory cache expires after 30 minutes, resets on location/timezone changes, and current weather is hidden on a mismatched date. Requests bypass URL caching via an ephemeral session. App links attribution and explains location/internet use.
- Added 20-byte BLE kinds 5 (integer weather/sun/date) and 6 (bounded city string). Firmware rejects malformed packets atomically and expires both after 60 seconds without updates. Phone enforces source cache expiry before every send. Missing/polar sun events remain unavailable independently.
- Swift decoder/serialization tests and production C++ parser tests passed, including actual Swift bytes decoded by firmware code, negative temperatures, timezone/date rollover, null sun events, invalid fields/city bounds and expiry. Existing clock and telemetry regression tests passed. A live Open-Meteo response at public Vancouver center coordinates decoded and encoded using the production Swift code successfully.
- Visually inspected actual 640 × 172 LVGL previews for normal weather, long city/thunderstorm label, and stale weather. Artifact `docs/previews/clock-weather.png` uses illustrative fixture values.
- V2 build/upload passed with verified hashes; SHA-256 `581cc0585aca0d174eaa044fe4a87fc941597e592e48066ec6b89819eff9274e`. Eight-second boot capture showed IMU/UI ready without panic.
- Signed iOS build, final installation and launch passed. Full physical location-to-BLE weather delivery and background behavior still require starting a phone session and user observation; host/API tests are not a substitute for that check.
- Logs: `logs/boot-weather-2026-11-14.log`, `logs/upload-weather-2026-11-14.log`, `logs/ios-weather-{build,install,launch}-2026-11-14.log`.

# Set Zero confirmation — 2026-11-14

- Camp SET ZERO now opens a modal prompt with CANCEL and CONFIRM. Only CONFIRM calls calibration; success/failure feedback remains on Camp. Modal blocks underlying navigation and gestures.
- Actual LVGL pointer simulations passed: opening/tapping background does not calibrate, cancel makes zero calibration calls, confirm makes exactly one call, invalid readings show failure, and inverted-landscape touch mapping works. Preview visually checked at 640 × 172 (`docs/previews/set-zero-confirm.png`).
- V2 build/upload passed with verified flash hashes. Eight-second boot capture showed IMU/UI ready without panic. Physical calibration was not changed during verification.
- Logs: `logs/upload-zero-confirm-2026-11-14.log`, `logs/boot-zero-confirm-2026-11-14.log`.

# Larger Explore readings — 2026-11-14

- Increased elevation, speed and pressure digits from 32 px to 48 px. Kept 190 px column bounds and a measured 32 px fallback for unusually long values; missing readings also use the larger font.
- Visually checked actual LVGL previews with 1284 / 8.5 / 1013.2 and -12345 / 120.5 / 1200.0; all fit without column overlap. Preview artifact: `docs/previews/explore-large.png`.
- V2 build/upload succeeded with verified flash hashes. Eight-second restart capture showed IMU/UI ready without panic.
- Logs: `logs/upload-explore-large-2026-11-14.log`, `logs/boot-explore-large-2026-11-14.log`.

# Fourth screen: phone-synced clock — 2026-11-14

- Added large vector HH:MM:SS digits and local date on CLOCK. Fourth dot/title/swipe navigation is available only while the phone is connected with a sync received within 60 seconds. Disconnect/expiry returns an active Clock screen to Explore. Orientation support applies to the entire screen.
- Added integer BLE kind 4: UTC milliseconds and current local UTC offset. iOS timestamps immediately before sending, updates every telemetry cycle even without GPS, and resyncs after reconnect. Firmware uses monotonic elapsed time between syncs; clock is cleared on disconnect and reboot.
- Host parser tests passed for integer precision, timezone extremes/fractional offsets, ticking, midnight rollover, expiry, disconnected/unsynced state and malformed packets. Existing telemetry tests passed.
- Actual LVGL 640 × 172 rendering visually verified (`docs/previews/clock.png`, illustrative time). Simulated disconnect while on Clock asserted fallback to Explore. Used vector digits to avoid transformed-text layer allocation.
- V2 build and upload succeeded with flash hash verification; firmware SHA-256 `45b1075ff2a844c2e7767af1fddb1bf2892a4d3591ce72595f0374913fba964c`. Eight-second reboot capture showed IMU/UI ready without panic.
- Signed iOS build, installation and launch succeeded. Live BLE time sync, phone timezone changes and background sync behavior still require physical session testing.
- Logs: `logs/boot-clock-2026-11-14.log`, `logs/upload-clock-2026-11-14.log`, and `logs/ios-clock-{build,install,launch}-2026-11-14.log`.

# Jeep iOS app icon — 2026-11-14

- Added an opaque 1024 × 1024 amber-on-dark app icon rendered from the dashboard's existing front Jeep drawing. `scripts/generate_app_icon.py` extracts the production front drawing and rasterizes it with Pillow; no dashboard screenshot scaling is used.
- Added AppIcon asset catalog to the Xcode resources phase and selected it for both build configurations. Visually inspected the generated artwork and confirmed compiled `CFBundleIcons` references AppIcon.
- Signed Debug build and installation on the connected iPhone succeeded. Home Screen appearance has not been observed remotely.
- Logs: `logs/ios-icon-build-2026-11-14.log`, `logs/ios-icon-install-2026-11-14.log`.

# iOS installation completed — 2026-11-14

- After the account holder accepted Apple's updated agreement, automatic provisioning and the signed iPhoneOS Debug build succeeded.
- CoreDevice successfully installed `com.trailstrip.prototype` on the connected physical iPhone and reported a successful application launch.
- Bluetooth, Location and Motion permission acceptance, Start session, and end-to-end telemetry with the gadget remain to be verified by the user.
- Logs: `logs/ios-signed-build-2026-11-14.log`, `logs/ios-install-2026-11-14.log`, `logs/ios-launch-2026-11-14.log`.

# iOS device installation attempt — 2026-11-14

- Connected paired physical iPhone detected by CoreDevice. Xcode installed its missing support components successfully.
- Current iPhoneOS Debug build passed with signing disabled. Product: `/private/tmp/trailstrip-ios-unsigned/Build/Products/Debug-iphoneos/Trailstrip.app`.
- Signed build for the connected iPhone failed: Apple requires the team's Account Holder to accept the updated Program License Agreement. The cached wildcard provisioning profile also lacks the current development signing certificate; automatic provisioning could not refresh it while account access is blocked.
- App not installed or launched. Requested account-holder action before retrying automatic provisioning and installation. Team selection was supplied as a build argument, without embedding account details in the project.
- Logs: `logs/ios-unsigned-build-2026-11-14.log`, `logs/ios-signed-build-2026-11-14.log`.

# Persistent display preferences — 2026-11-14

- Store landscape orientation and Roll front/rear choice together in NVS `trailstrip/ui_flags_v1`. Restore before display, sensor and UI initialization. Defaults are normal/front when absent or invalid; auto-rotation remains active after boot. Unchanged preferences do not write flash.
- Host tests of the production persistence module passed: all four combinations survive reload, changing either preserves the other, repeated no-op setters avoid commits, failed writes retain committed state, invalid data defaults, and restored orientation still auto-rotates.
- Actual LVGL preview of rear/inverted restored at startup is byte-identical to rear/inverted selected by touch.
- V2 build/upload succeeded with flash hash verification. Firmware SHA-256: `83b179850560909c4fb0e8cb5b5ef50d1cf42139825b0359547bdf44f53179c8`.
- Eight-second hardware restart capture showed preferences restored before display initialization and IMU/UI ready without panic. Physical selection followed by power cycling is still to be confirmed on the device; non-default persistence combinations were verified with the host NVS backend.
- Logs: `logs/boot-preferences-2026-11-14.log`, `logs/upload-preferences-2026-11-14.log`.

# Landscape-only auto-rotation — 2026-11-14

- Added normal/inverted landscape selection from uncalibrated accelerometer gravity. Requires 600 ms of continuous confident samples, norm 0.85–1.15 g, |Y| ≥ 0.75 of norm and |X| ≤ 0.5 of norm. Ambiguous portrait/flat poses, shocks, missing data and sample gaps reset the candidate without changing the selected landscape.
- Rotates the full framebuffer 180° during the existing native-panel copy. Resolution remains 640 × 172; no portrait layout or extra framebuffer. Touch coordinates use the same inverse mapping. An active touch must release after an orientation change.
- Remaps sensor X/Y in inverted landscape before filtering/calibration; pitch Z is unchanged. Resets the attitude filter on orientation changes to avoid blending across coordinate frames. Existing calibration and Roll front/rear choice are retained.
- Host tests passed: both orientation transitions, debounce, portrait/flat/shock rejection, sample gaps, all 110,080 framebuffer pixels and touch coordinates in both orientations, and equivalent sensor mounts. Existing attitude regression tests also passed.
- Actual LVGL headless preview: rotated framebuffer equals a pixel-exact 180° rotation of normal; simulated physical taps select the rear Roll view in either orientation.
- V2 build/upload passed and flash hashes verified; firmware SHA-256 `f921184b88b80cab2a84e8d87f34e3669f41f13ae176673fdaba0b32c48d9db3`. Fifteen-second startup capture showed IMU/UI ready and no panic. No physical flip occurred in the captured log, so on-device auto-rotation and touch behavior still require user confirmation.
- Logs: `logs/boot-auto-rotation-2026-11-14.log`, `logs/upload-auto-rotation-2026-11-14.log`.

# Tap to switch front/rear Roll view — 2026-11-14

- Tap the Roll Jeep in Trail to toggle front/rear. Rear illustration includes back window, square tail lamps and spare tire; its visual roll angle is the negative of the front view angle. Numeric roll, gauge, pitch and Camp behavior are unchanged.
- Added FRONT / TAP and REAR / TAP indicator. View survives page changes within a session and defaults to front on reboot. A swipe is not treated as a toggle.
- Actual LVGL pointer simulation: one tap selects rear, two taps restore a byte-identical front framebuffer. At +15° the two illustrations lean opposite ways. Framebuffer comparison confirms that only the Roll drawing and view label change.
- Preview artifacts: `docs/previews/trail-front.png`, `docs/previews/trail-rear.png` (sample +15° roll/+12° pitch).
- V2 build/upload passed with hash verification; SHA-256 `df18aba704d0c35375d50f7a616b7e2c8e25b3582aedc15ce174061c754c8bb8`. Eight-second startup log shows display, IMU and UI ready without a panic. Physical touch confirmation remains with the user.
- Logs: `logs/boot-rear-view-2026-11-14.log`, `logs/upload-rear-view-2026-11-14.log`.

# Camp bubble pitch direction — 2026-11-14

- Changed only the Camp bubble vertical projection from center + pitch to center - pitch because screen Y increases downward. Roll movement, attitude math, numerical readings, Trail animations and saved zero are unchanged.
- Actual LVGL previews at roll +2° / pitch +5° and -5° show identical horizontal positions, with positive pitch above center and negative pitch below.
- V2 build/upload passed with flash hash verification. Firmware SHA-256: `0c859d10d3848db5d39acb19873ae3baf7332ba51f7f37d7b5ad0f8cc0b30012`.
- 8-second startup capture shows IMU and UI ready, no panic. Physical bubble response remains for user confirmation.
- Logs: `logs/boot-camp-bubble-2026-11-14.log`, `logs/upload-camp-bubble-2026-11-14.log`.

# Upright mount and pitch direction — 2026-11-14

- Upright driver-facing mount uses negative sensor Y as vertical reference. User reported pitch reversed after the initial upright mapping; pitch now uses negative sensor Z. Roll is unchanged.
- Converted legacy upright pitch calibration on read and save new calibrations under `zero_mount_v2`, preserving the physical zero. Flat-board calibration is not used.
- Host regression tests pass for upright zero, independent axes, both signs, compound tilts and old calibration conversion.
- V2 build/upload succeeded with verified flash hashes. Firmware SHA-256: `5380bce59135f793754d532ef861b91bfb7d5df3f71f860029a7357a9a83a795`.
- 10-second boot capture shows display/IMU initialized and UI ready, without a panic. First sensor sample: (0.469, -0.600, 0.382) g; reported roll -38.0°, pitch -26.7°. This was not a verified level pose; physical nose-up/down response remains for the user to confirm.
- Logs: `logs/boot-pitch-2026-11-14.log` and `logs/upload-pitch-2026-11-14.log`.

# Jeep Trail UI correction — 2026-11-13

- Replaced placeholder tilt lines with front and four-door side Jeep line art, seven-slot grille, headlights, wheels, windows, mirrors, and external spare. Drawings rotate with live roll/pitch; missing sensor data dims them.
- Added angle scales, large numerical values, center divider, reference-style header/footer, and three page dots. Navigation: swipe, tap the title to cycle, or tap a dot.
- Rendered the actual `ui.cpp` and `vehicle.cpp` with a desktop LVGL framebuffer at 640 × 172. Visually inspected 8°/12°, -30°/30°, -88°/-179°, missing data, and Camp. Large values adapt font size; vehicle rotation is visually clamped to ±45° and gauge pointers to ±30° without clamping numerical readings.
- Firmware build and V2 upload passed; written hashes verified. Application: 991,296 bytes; SHA-256 `44cdb40c0f3926b53fe06619fa79bcaf8848d2d79fe5ac2749f2e32cfb5afc9b`.
- 20-second hardware startup capture passed: display and IMU initialized, BLE advertising active, `UI ready 640x172; animated Jeep views; BLE telemetry available`, no panic observed. Log: `logs/boot-jeep-ui-2026-11-13.log`.
- Device screen appearance and touch behavior still require user observation. Desktop preview uses illustrative sensor values; production firmware uses the real sensor.

# Hardware upload — 2026-11-13

- User-confirmed V2 detected at `/dev/cu.usbmodem2101` (USB 303a:1001).
- esptool identified ESP32-S3 revision v0.2, 16 MB flash, and 8 MB embedded PSRAM.
- Saved all 16,777,216 bytes of original flash to `backups/waveshare-v2-before-trailstrip-2026-11-13.bin` before upload. SHA-256: `20b951d265bb228784e6a7509a4552afbf6d3ed343b613a357d9cc86a6ca3bb5`. Backup is ignored by Git because original flash may contain private settings.
- PlatformIO V2 build and upload succeeded; esptool verified written data hashes. Application binary SHA-256 remains `4669fea95f787f18073eaf9c7922144547634347248e73824ed3dd7abfd6c34b`.
- Captured a 20-second USB startup log after resetting the device: PSRAM memory test OK, AXS15231B panel initialized, QMI8658 ready, NimBLE advertising started, UI ready at 640 × 172. No panic or application crash observed in this capture. Initial partial reset messages arose while opening/resetting the serial connection.
- Logs saved locally in `logs/boot-2026-11-13.log` and `logs/upload-2026-11-13.log`.
- Screen appearance, touch operation, physical tilt accuracy, calibration persistence, and iPhone telemetry have NOT yet been confirmed on hardware. Boot logs confirm initialization, not visual correctness or end-to-end sensor accuracy.

# Initial implementation validation — 2026-11-09

## Host verification

- Telemetry parser host tests: passed (valid/invalid packet lengths and versions, validity flags, nonfinite values, physical units/ranges, and 5-second expiration).
- iPhone app: unsigned arm64 iPhoneOS Debug build succeeded with Xcode 27.0. No signing team selected; no app installed on a phone.
- Info.plist and Xcode project syntax: passed `plutil -lint`.
- Firmware: final PlatformIO builds passed for both `trailstrip_v2` and `trailstrip_v1` (ESP-IDF 5.5.3); no compiler warnings in the final build log.

## Hardware status

No ESP32 appeared in `/dev/cu.*`, Python serial enumeration, or the macOS IOUSB tree, including a check outside the sandbox. Only Bluetooth/debug serial entries were visible. No firmware was uploaded; no flash contents or fuses were modified.

The user confirmed the case label says V2 on 2026-11-09. Use `trailstrip_v2` for this device. USB serial discovery was repeated after confirmation and still found no device. The V1/V2 implementations follow their corresponding vendor pin/reset configuration but neither has been exercised on the user's hardware.

## Required bench checks when USB is available

1. Confirm case/PCB revision and detected flash size; preserve existing firmware before replacing if needed.
2. Flash the matching environment and capture USB boot logs. Check PSRAM initialization, display, touch coordinates, and QMI8658 detection.
3. Check all three views, tab selection/swipes, and values at known sensor orientations. Verify SET ZERO persists after reboot.
4. Establish the actual installed sensor-to-vehicle axis transform before using pitch/roll in the Jeep. Add gyro fusion and dynamic-motion handling after that mapping is known.
5. Install/sign the iPhone app; verify elevation, speed, and pressure units against phone readings. Disconnect/reconnect and verify fields clear/expire.
6. Lock the iPhone screen for an extended session. Test each sensor's delivery independently, including missing barometer callbacks. Test stop/start, Bluetooth toggle, permission denial, and force quit.
7. Finish detailed Jeep graphics, separate camp/mount references, brightness controls, authorized pairing, and longer-duration power/thermal tests.

Xcode's build log reported mismatched CoreDevice/CoreSimulator services on this Mac. The arm64 iPhoneOS compile succeeded, but simulator/device deployment was not validated; these local tooling warnings may need attention when installing on a physical phone.

## Firmware artifacts

- `.pio/build/trailstrip_v1/firmware.bin`: 959,776 bytes; SHA-256 `cf267234b7eca94de1dfa6aaf8d2f71076b35606e3588d884d22f70680feaacd`.
- `.pio/build/trailstrip_v2/firmware.bin`: 964,752 bytes; SHA-256 `4669fea95f787f18073eaf9c7922144547634347248e73824ed3dd7abfd6c34b`.

Application binaries alone are not a full flash image; use PlatformIO upload to include the matching bootloader and partition table. These artifacts are build-verified only.
