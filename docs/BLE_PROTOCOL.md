# Trailstrip BLE protocol v1

The ESP32 is a connectable peripheral/GATT server. iOS is the central/GATT client.

- Advertised name (scan response): `TRAILSTRIP`
- Service: `A87E0001-B5A3-F393-E0A9-E50E24DCCA9E`
- Receive characteristic: `A87E0002-B5A3-F393-E0A9-E50E24DCCA9E`
- Operation: write with response, exactly 20 bytes. Compatible with the default ATT MTU.
- Custom Trailstrip service; not Nordic UART. Discover the characteristic and send only the specified packets.

For sensor kinds 1–3, byte 0: version `1`. Byte 1: kind `1`, `2`, or `3`. Byte 2: validity mask (bits 0–3 for four fields). Byte 3: reserved, must be zero. Bytes 4–19: four IEEE-754 float32 values, little endian. Invalid slots should contain zero and have their mask bit clear. Nonfinite valid values and malformed packets are rejected atomically with an ATT error.

| Kind | Field 0 | Field 1 | Field 2 | Field 3 |
|---|---|---|---|---|
| 1 | Elevation m above sea level | Speed m/s, nonnegative | Course degrees true [0,360) | Horizontal accuracy m, nonnegative |
| 2 | Ambient pressure hPa, 100–1200 | Relative altitude m | Location vertical accuracy m, nonnegative | Reserved/invalid |
| 3 | Camp distance m, nonnegative | Camp bearing degrees true [0,360) | Trip distance m, nonnegative | Reserved/invalid |

The phone converts Core Motion pressure from kPa to hPa. Course describes movement, not a stationary compass. Camp bearing is absolute true-north bearing, not a vehicle-relative steering instruction. Trip distance is a rough session estimate with a basic accuracy/jitter gate, not an odometer.

Each received field expires after 5 seconds. Invalid bits clear previous values immediately. Disconnect clears all values. Receivers use local monotonic receipt time; senders MUST independently enforce sensor freshness, otherwise repeated old samples could appear current. The iPhone implementation uses CLLocation timestamp and CMAltitudeData monotonic timestamp. Its periodic timer aims at 1 Hz while running; background execution does not guarantee that cadence.

No encryption requirement or peer bonding is configured in this bench prototype. No commands to vehicle systems exist. User-controlled pairing and authorization should precede broader use.

## Phone clock (kind 4)

Uses the same characteristic and 20-byte write, with integer fields instead of float32:

| Bytes | Meaning |
|---|---|
| 0–3 | Version 1, kind 4, flags 1, reserved 0 |
| 4–11 | UTC Unix milliseconds, uint64 little endian |
| 12–15 | Local UTC offset in seconds, int32 little endian, including current DST |
| 16–19 | Reserved, all zero |

Supported UTC dates are 2020–2099, with offsets from −12 to +14 hours. Invalid packets are rejected without changing the previous clock. The iPhone creates the timestamp when sending the packet, refreshing it with each telemetry cycle even without a GPS fix. The gadget advances time using its monotonic timer between packets, adds the supplied timezone offset, and displays a 24-hour clock and local date. Subsequent packets update manual time changes and timezone/DST changes.

The Clock page and fourth navigation dot are available only while BLE is connected and the latest clock sync is at most 60 seconds old. Disconnect clears the clock immediately; expiry or disconnect on the Clock page returns to Explore. A new connection or reboot requires a new sync. Clock time is not persisted to NVS. Older phones sending only kinds 1–3 continue to work, without the Clock page.

## Clock weather (kind 5) and city (kind 6)

Both remain version 1 / 20-byte writes; byte 3 must be zero. Kind 5 uses integer fields:

| Bytes | Meaning |
|---|---|
| 2 | Validity bits: 0 temperature, 1 daily forecast/high/low, 2 sunrise, 3 sunset; upper bits zero |
| 4–5, 6–7, 8–9 | Current, high, low temperature: signed int16 LE, tenths Celsius, −1000…700 |
| 10–11, 12–13 | Sunrise/sunset: uint16 LE minutes after midnight (0…1439), in phone timezone |
| 14 | Daily WMO weather code; unknown codes display FORECAST |
| 15 | Daily maximum precipitation probability, 0…100; 255 unavailable |
| 16–19 | uint32 LE date YYYYMMDD in phone timezone |

A zero mask clears weather. Invalid valid-field ranges reject the packet atomically. Absent sun events have their bits clear. Weather expires after 60 seconds without a packet, and must match the synced local calendar date. The phone also enforces a 30-minute cache lifetime before sending, so resends do not make old data fresh.

Kind 6: byte 2 is 1 for valid city or 0 to clear. Bytes 4–19 contain up to 16 printable ASCII bytes, zero-padded (no nonzero bytes after padding). iOS transliterates names and truncates to 16 bytes for the display; its app retains the full city name. City expires after 60 seconds without a packet. Disconnect clears weather and city along with other telemetry.

The iPhone requests city reverse-geocoding from Apple and forecast data from Open-Meteo using a fresh location accurate to 1 km. Coordinates sent to Open-Meteo are rounded to three decimals. Requests are throttled to at most once per minute; normal refresh is every 15 minutes, or on movement of at least 5 km, date rollover, or phone timezone change. Movement/timezone changes invalidate the old location's cache. Retries retain data only within its 30-minute lifetime. Both lookups require internet. Time sync remains independent of weather/GPS success.

API reference: https://open-meteo.com/en/docs . Weather attribution is linked in the iPhone app (Open-Meteo, CC BY 4.0). The free endpoint is for non-commercial use; a commercial deployment must select an appropriate service plan.
