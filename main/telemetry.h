#pragma once
#include <cstdint>
#include <cstddef>
struct Reading { float value=0; uint64_t received_ms=0; bool valid=false; };
struct PhoneClock {
    uint64_t unix_ms=0, received_ms=0;
    int32_t utc_offset_seconds=0;
    bool valid=false;
};
bool clock_local_seconds(const PhoneClock &clock, bool connected, uint64_t now, int64_t &seconds);
struct Telemetry {
    PhoneClock clock;
    Reading altitude, speed, course, location_accuracy;
    Reading pressure, relative_altitude, vertical_accuracy;
    Reading camp_distance, camp_bearing, trip_distance;
};
bool decode_packet(const uint8_t *bytes, size_t size, uint64_t now, Telemetry &state);
bool fresh(const Reading &r, uint64_t now);
Telemetry telemetry_snapshot();
bool telemetry_receive(const uint8_t *data, size_t len);
void telemetry_disconnect();
bool phone_connected();
void telemetry_connect(bool connected);
