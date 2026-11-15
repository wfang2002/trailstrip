#pragma once
#include <cstdint>
#include <cstddef>
struct Reading { float value=0; uint64_t received_ms=0; bool valid=false; };
struct Telemetry {
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
