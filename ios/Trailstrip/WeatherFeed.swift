import Foundation
import CoreLocation

struct WeatherSnapshot {
    let day: UInt32
    let temperature: Double?
    let high: Double?
    let low: Double?
    let sunrise: UInt16?
    let sunset: UInt16?
    let code: UInt8
    let precipitation: UInt8
    let fetchedAt: Date

    struct Response: Decodable {
        struct Current: Decodable { let time: String; let temperature_2m: Double? }
        struct Daily: Decodable {
            let time: [String]
            let temperature_2m_max: [Double?]
            let temperature_2m_min: [Double?]
            let sunrise: [String?]
            let sunset: [String?]
            let weather_code: [Int?]
            let precipitation_probability_max: [Int?]
        }
        let current: Current
        let daily: Daily
    }
    static func dateKey(_ date: Date, zone: TimeZone) -> UInt32 {
        var calendar = Calendar(identifier: .gregorian); calendar.timeZone = zone
        let c = calendar.dateComponents([.year, .month, .day], from: date)
        return UInt32(c.year! * 10000 + c.month! * 100 + c.day!)
    }
    static func decode(_ data: Data, now: Date, zone: TimeZone) throws -> WeatherSnapshot {
        let r = try JSONDecoder().decode(Response.self, from: data)
        let formatter = DateFormatter(); formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = zone; formatter.dateFormat = "yyyy-MM-dd"
        guard let dayText = r.daily.time.first, let day = formatter.date(from: dayText),
              dateKey(day, zone: zone) == dateKey(now, zone: zone) else { throw URLError(.cannotParseResponse) }
        formatter.dateFormat = "yyyy-MM-dd'T'HH:mm"
        func minutes(_ values: [String?]) -> UInt16? {
            guard let first = values.first, let text = first, let date = formatter.date(from: text),
                  dateKey(date, zone: zone) == dateKey(now, zone: zone) else { return nil }
            var calendar = Calendar(identifier: .gregorian); calendar.timeZone = zone
            return UInt16(calendar.component(.hour, from: date) * 60 + calendar.component(.minute, from: date))
        }
        func temperature(_ value: Double?) -> Double? {
            guard let v = value, v.isFinite, (-100...70).contains(v) else { return nil }; return v
        }
        let currentTime = formatter.date(from: r.current.time)
        let currentFresh = currentTime.map { abs(now.timeIntervalSince($0)) <= 5400 } ?? false
        let code = r.daily.weather_code.first.flatMap { $0 }.flatMap { UInt8(exactly: $0) } ?? 255
        let rain = r.daily.precipitation_probability_max.first.flatMap { $0 }
        return WeatherSnapshot(day: dateKey(day, zone: zone), temperature: currentFresh ? temperature(r.current.temperature_2m) : nil,
            high: temperature(r.daily.temperature_2m_max.first.flatMap { $0 }), low: temperature(r.daily.temperature_2m_min.first.flatMap { $0 }),
            sunrise: minutes(r.daily.sunrise), sunset: minutes(r.daily.sunset), code: code,
            precipitation: rain.flatMap { (0...100).contains($0) ? UInt8($0) : nil } ?? 255, fetchedAt: now)
    }
    func packet(now: Date, zone: TimeZone) -> Data {
        guard now.timeIntervalSince(fetchedAt) >= 0, now.timeIntervalSince(fetchedAt) <= 1800,
              Self.dateKey(now, zone: zone) == day else { return Data([1,5,0,0] + Array(repeating: 0, count: 16)) }
        var flags: UInt8 = 0
        if temperature != nil { flags |= 1 }
        if let h = high, let l = low, h >= l { flags |= 2 }
        if sunrise != nil { flags |= 4 }; if sunset != nil { flags |= 8 }
        var data = Data([1,5,flags,0])
        func append16(_ value: UInt16) { var little = value.littleEndian; withUnsafeBytes(of: &little) { data.append(contentsOf: $0) } }
        for t in [temperature, high, low] { append16(UInt16(bitPattern: Int16(((t ?? 0) * 10).rounded()))) }
        append16(sunrise ?? 0); append16(sunset ?? 0)
        data.append(code); data.append(precipitation)
        var little = day.littleEndian; withUnsafeBytes(of: &little) { data.append(contentsOf: $0) }
        return data
    }
}

// In-memory forecast cache, accessed on the main queue like TrailSession.
final class WeatherFeed {
    private let geocoder = CLGeocoder()
    private let session = URLSession(configuration: .ephemeral)
    private var task: Task<Void, Never>?
    private var generation = UUID()
    private var sourceLocation: CLLocation?
    private var lastAttempt = Date.distantPast
    private var zoneID = ""
    private(set) var snapshot: WeatherSnapshot?
    private(set) var city = ""
    private var cityDate = Date.distantPast
    private(set) var status = "Start a session for local weather"

    func stop() {
        generation = UUID(); task?.cancel(); task = nil; geocoder.cancelGeocode()
        snapshot = nil; city = ""; sourceLocation = nil; lastAttempt = .distantPast
        status = "Weather session stopped"
    }
    func refresh(location: CLLocation) {
        let now = Date(), zone = TimeZone.current
        guard location.horizontalAccuracy >= 0, location.horizontalAccuracy <= 1000,
              abs(location.timestamp.timeIntervalSinceNow) < 5 else { return }
        let moved = sourceLocation.map { location.distance(from: $0) >= 5000 } ?? false
        let changedZone = zoneID != zone.identifier
        if moved || changedZone {
            generation = UUID(); task?.cancel(); task = nil; geocoder.cancelGeocode()
            snapshot = nil; city = ""; sourceLocation = location; zoneID = zone.identifier
        }
        guard task == nil, now.timeIntervalSince(lastAttempt) >= 60 else { return }
        let refreshDue = snapshot.map { now.timeIntervalSince($0.fetchedAt) >= 900 || $0.day != WeatherSnapshot.dateKey(now, zone: zone) } ?? true
        guard refreshDue || city.isEmpty else { return }
        lastAttempt = now; sourceLocation = location; zoneID = zone.identifier
        let token = generation
        status = "Updating city and forecast…"
        task = Task { @MainActor [weak self] in
            guard let self = self else { return }
            // City lookup failure must not prevent the forecast request.
            do {
                let places = try await self.geocoder.reverseGeocodeLocation(location, preferredLocale: Locale(identifier: "en_US"))
                guard token == self.generation, !Task.isCancelled else { return }
                self.city = places.first?.locality ?? places.first?.subAdministrativeArea ?? places.first?.administrativeArea ?? ""
                self.cityDate = Date()
            } catch { /* Retain only a still-fresh city from this location. */ }
            guard token == self.generation, !Task.isCancelled else { return }
            do {
                var url = URLComponents(string: "https://api.open-meteo.com/v1/forecast")!
                url.queryItems = [
                    URLQueryItem(name: "latitude", value: String(format: "%.3f", locale: Locale(identifier: "en_US_POSIX"), location.coordinate.latitude)),
                    URLQueryItem(name: "longitude", value: String(format: "%.3f", locale: Locale(identifier: "en_US_POSIX"), location.coordinate.longitude)),
                    URLQueryItem(name: "current", value: "temperature_2m"),
                    URLQueryItem(name: "daily", value: "weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max"),
                    URLQueryItem(name: "timezone", value: zone.identifier),
                    URLQueryItem(name: "forecast_days", value: "1")]
                var request = URLRequest(url: url.url!, cachePolicy: .reloadIgnoringLocalCacheData); request.timeoutInterval = 20
                let (data, response) = try await self.session.data(for: request)
                guard let http = response as? HTTPURLResponse, http.statusCode == 200 else { throw URLError(.badServerResponse) }
                let weather = try WeatherSnapshot.decode(data, now: Date(), zone: zone)
                guard token == self.generation, !Task.isCancelled else { return }
                self.snapshot = weather; self.status = "Forecast updated · Open-Meteo"
            } catch {
                guard token == self.generation, !Task.isCancelled else { return }
                self.status = "Weather unavailable · retrying in 1 minute"
            }
            self.task = nil
        }
    }
    func packets(now: Date = Date()) -> [UInt8: Data] {
        let empty = Data([1,5,0,0] + Array(repeating: 0, count: 16))
        let weather = zoneID == TimeZone.current.identifier ? (snapshot?.packet(now: now, zone: .current) ?? empty) : empty
        let cityFresh = now.timeIntervalSince(cityDate) >= 0 && now.timeIntervalSince(cityDate) <= 1800
        let latin = city.applyingTransform(.toLatin, reverse: false)?.folding(options: .diacriticInsensitive, locale: Locale(identifier: "en_US")) ?? city
        let bytes = Array(latin.utf8.filter { $0 >= 32 && $0 <= 126 }.prefix(16))
        let good = cityFresh && !bytes.isEmpty
        var cityPacket = Data([1,6,good ? 1 : 0,0])
        cityPacket.append(contentsOf: good ? bytes : [])
        cityPacket.append(contentsOf: Array(repeating: 0, count: 20-cityPacket.count))
        return [5: weather, 6: cityPacket]
    }
}
