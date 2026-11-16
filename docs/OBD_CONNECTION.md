# Veepeak direct BLE connection

The ESP32 is a BLE **central** for the Veepeak and remains a BLE **peripheral** for the iPhone. Two NimBLE connection slots are enabled. Each connection has its own handles, callbacks, retry state and data. Ordinary scanner errors disconnect only the scanner; a stuck termination may reset the shared BLE host and briefly disconnect both links.

## Setup for the first in-vehicle check

1. Plug the Veepeak OBDCheck BLE into the Jeep with ignition off, following its manual.
2. While parked, turn ignition on and power the gadget.
3. Close Car Scanner, JScan or any other app using the Veepeak. The Trailstrip iPhone app does not connect to the scanner.
4. Select the fifth dot / VEHICLE screen. It is available without a phone; navigation skips CLOCK when phone time is unavailable.
5. Expect SEARCHING FOR VEEPEAK → CONNECTING → DISCOVERING → INITIALIZING → live values, or a specific retry/error status. Auto protocol detection may take up to 20 seconds.

The scanner was unavailable during implementation. Live Veepeak service discovery, Jeep supported PIDs, radio coexistence, ignition-off/on behavior and disconnect/reconnect still need this test. Do not interpret the supplied desktop preview values as live vehicle data.

## Discovery and recovery

- Active scan: four seconds, then ten seconds idle; low scan duty to leave radio time for the phone. Only names matching VEEPEAK (case-insensitive, optionally a space/hyphen/underscore suffix) are candidates. If multiple Veepeaks are nearby, the first discovered wins; no persistent adapter selection is implemented yet.
- Verify service FFF0, notifying characteristic FFF1, writable characteristic FFF2, and the actual FFF1 CCCD. Enable notifications before issuing commands. Profile differences report an error rather than guessing writable characteristics.
- Support write-with-response or write-without-response according to discovered properties. Commands fit the default ATT payload; notifications are accumulated until the ELM `>` prompt.
- Connection: 10-second timeout. GATT discovery/subscription: bounded 10-second phases. Responses: 5 seconds, or 20 seconds for the initial protocol/PID discovery. Reconnect on timeout to prevent a late response being assigned to another command. Stuck termination: shared BLE-host recovery after 10 seconds.
- No scanner nearby is a normal state and does not reset the phone link. The phone's independent two-minute idle cleanup still applies only to the phone handle.

## Read-only commands and readings

Adapter-local initialization: `ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0`. These reset/configure the adapter, not the vehicle ECU.

| Query | Purpose | Display |
|---|---|---|
| `0100` | Supported standard PIDs 01–20 | Used to gate requests |
| `0105` | Engine coolant temperature, A−40 | °C |
| `010C` | Engine speed, (256A+B)/4 | RPM |
| `010D` | Vehicle speed, A | km/h |
| `ATRV` | Adapter supply voltage | OBD volts |

Only supported vehicle PIDs are polled, serially with a 250 ms minimum gap after each prompt. Voltage can remain available when the ECU is unavailable; supported-PID discovery retries every 10 seconds when it has not succeeded. Each reading expires after 5 seconds. NO DATA, malformed and missing readings display dashes; zero is shown only for valid zero data.

No DTC clearing, actuator control, ECU coding, manufacturer-specific diagnostic sessions, TPMS writes or security-gateway bypass is implemented. Transmission temperature and individual tire pressures are not part of this first version.

## Validation tools

```sh
clang++ -std=c++17 -Wall -Wextra -Imain tests/obd_test.cpp main/obd.cpp -o /private/tmp/obd-test
/private/tmp/obd-test
TRAILSTRIP_PREVIEW_OBD=live /private/tmp/trailstrip-ui-preview/preview /private/tmp/vehicle.ppm 0 0 4 1 0 0 0 1
```

Use `TRAILSTRIP_PREVIEW_OBD=stale` for expired samples. With no phone time, three title taps reach Vehicle (Clock is skipped). USB logs use tag `obd` for scanner discovery, handles and failure reasons; phone events retain tag `ble`.

## References

- [Veepeak setup instructions](https://veepeak.com/pages/obdcheck-ble-user-instructions)
- [ELM327 command/data sheet](https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf)
- [First-hand Veepeak FFF0/FFF1/FFF2 transport observation](https://github.com/roosht3/tpms-relearn/blob/main/docs/protocol.md) — used for the BLE profile only, not that project's vehicle-specific commands. Profile still requires verification on this user's adapter.
- NimBLE client APIs verified against local ESP-IDF 5.5.3 headers.
