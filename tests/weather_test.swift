import Foundation
@main struct WeatherTests {
    static func main() throws {
        let now = ISO8601DateFormatter().date(from: "2026-11-14T20:00:00Z")!
        let zone = TimeZone(identifier: "America/Vancouver")!
        let json = """
        {"current":{"time":"2026-11-14T12:00","temperature_2m":8.5},"daily":{"time":["2026-11-14"],"temperature_2m_max":[12],"temperature_2m_min":[6],"sunrise":["2026-11-14T07:24"],"sunset":["2026-11-14T16:31"],"weather_code":[61],"precipitation_probability_max":[80]}}
        """
        let w = try WeatherSnapshot.decode(Data(json.utf8), now: now, zone: zone)
        assert(w.day == 20261114 && w.sunrise == 444 && w.sunset == 991)
        let bytes = w.packet(now: now, zone: zone)
        assert(bytes.count == 20 && bytes[2] == 15 && bytes[4] == 85 && bytes[14] == 61 && bytes[15] == 80)
        try bytes.write(to: URL(fileURLWithPath: "/private/tmp/trailstrip-swift-weather.bin"))
        assert(w.packet(now: now.addingTimeInterval(1801), zone: zone)[2] == 0)
        assert(w.packet(now: now.addingTimeInterval(-1), zone: zone)[2] == 0)
        assert(w.packet(now: now, zone: TimeZone(secondsFromGMT: 14*3600)!)[2] == 0)
        let polar = json.replacingOccurrences(of: "\"2026-11-14T07:24\"", with: "null").replacingOccurrences(of: "\"2026-11-14T16:31\"", with: "null").replacingOccurrences(of: "8.5", with: "-23.5")
        let p = try WeatherSnapshot.decode(Data(polar.utf8), now: now, zone: zone)
        assert(p.sunrise == nil && p.sunset == nil && p.packet(now: now, zone: zone)[2] == 3)
        let old = try WeatherSnapshot.decode(Data(json.replacingOccurrences(of: "T12:00", with: "T08:00").utf8), now: now, zone: zone)
        assert(old.temperature == nil && old.packet(now: now, zone: zone)[2] == 14)
        do { _ = try WeatherSnapshot.decode(Data(json.utf8), now: now.addingTimeInterval(86400), zone: zone); assertionFailure("wrong day accepted") } catch {}
        do { _ = try WeatherSnapshot.decode(Data("{}".utf8), now: now, zone: zone); assertionFailure("bad JSON accepted") } catch {}
        if CommandLine.arguments.count > 1 {
            let live = try WeatherSnapshot.decode(Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1])), now: Date(), zone: zone)
            assert(live.packet(now: Date(), zone: zone).count == 20)
            print("live API response decoded and encoded successfully")
        }
        print("weather Swift: decode, integer encoding, stale cache, timezone date, missing sun events, old current data and malformed response passed")
    }
}
