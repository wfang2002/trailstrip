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
