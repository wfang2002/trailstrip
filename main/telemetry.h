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
struct Weather {
    int16_t temperature=0, high=0, low=0; // tenths Celsius
    uint16_t sunrise=0, sunset=0; // phone-local minutes after midnight
    uint8_t flags=0, code=255, precipitation=255;
    uint32_t date=0; // YYYYMMDD in the clock's timezone
    uint64_t received_ms=0;
};
bool weather_fresh(const Weather &weather, const PhoneClock &clock, bool connected, uint64_t now);
const char *weather_description(uint8_t code);
struct Telemetry {
    Weather weather;
    char city[17]={};
    uint64_t city_received_ms=0;
    bool city_valid=false;
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
