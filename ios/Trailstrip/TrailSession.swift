import Foundation
import Combine
import CoreBluetooth
import CoreLocation
import CoreMotion

final class TrailSession: NSObject, ObservableObject, CLLocationManagerDelegate, CBCentralManagerDelegate, CBPeripheralDelegate {
    @Published var running = false
    @Published var bluetoothStatus = "Session stopped"
    @Published var sensorStatus = "Location and motion permissions required"
    @Published var elevationText = "--"
    @Published var speedText = "--"
    @Published var pressureText = "--"
    @Published var weatherStatus = "Start a session for local weather"
    @Published var cityText = "--"
    private let weather = WeatherFeed()
    @Published var campStatus = "No camp saved"
    private let location = CLLocationManager()
    private let altimeter = CMAltimeter()
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var receiver: CBCharacteristic?
    private var timer: Timer?
    private var fix: CLLocation?
    private var barometer: CMAltitudeData?
    private var camp: CLLocation?
    private var previousFix: CLLocation?
    private var trip = 0.0
    private var pending = [UInt8: Data]()
    private var writing = false
    private var link = BLELinkState()
    private var uptime: TimeInterval { ProcessInfo.processInfo.systemUptime }
    private let serviceID = CBUUID(string: "A87E0001-B5A3-F393-E0A9-E50E24DCCA9E")
    private let rxID = CBUUID(string: "A87E0002-B5A3-F393-E0A9-E50E24DCCA9E")

    override init() {
        super.init()
        location.delegate = self
        location.desiredAccuracy = kCLLocationAccuracyBest
        location.distanceFilter = kCLDistanceFilterNone
        location.activityType = .automotiveNavigation
        central = CBCentralManager(delegate: self, queue: .main)
        if let coordinates = UserDefaults.standard.array(forKey: "camp") as? [Double], coordinates.count == 2 {
            camp = CLLocation(latitude: coordinates[0], longitude: coordinates[1])
            campStatus = "Saved camp restored"
        }
    }
    func start() {
        guard !running else { return }
        running = true
        fix = nil; barometer = nil; previousFix = nil; trip = 0
        location.requestWhenInUseAuthorization()
        startLocationIfAuthorized()
        if CMAltimeter.isRelativeAltitudeAvailable() {
            altimeter.startRelativeAltitudeUpdates(to: .main) { [weak self] data, error in
                guard let self = self, self.running else { return }
                self.barometer = data
                if let error = error { self.sensorStatus = "Barometer: \(error.localizedDescription)" }
                self.publishReadings()
            }
        }
        scan()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in self?.maintainConnection(); self?.publishReadings() }
    }
    func stop() {
        running = false
        weather.stop(); weatherStatus = weather.status; cityText = "--"
        timer?.invalidate(); timer = nil
        location.stopUpdatingLocation(); altimeter.stopRelativeAltitudeUpdates()
        central.stopScan()
        cancelLink("Session stopped")
        receiver = nil; pending.removeAll(); writing = false
        fix = nil; barometer = nil
        elevationText = "--"; speedText = "--"; pressureText = "--"
        bluetoothStatus = "Session stopped"
    }
    private func startLocationIfAuthorized() {
        guard running else { return }
        switch location.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            location.allowsBackgroundLocationUpdates = true
            location.showsBackgroundLocationIndicator = true
            location.pausesLocationUpdatesAutomatically = false
            location.startUpdatingLocation()
            sensorStatus = "Acquiring location"
        case .denied, .restricted: sensorStatus = "Enable Location permission in Settings"
        default: break
        }
    }
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) { startLocationIfAuthorized() }
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) { sensorStatus = error.localizedDescription }
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard running, let latest = locations.last, abs(latest.timestamp.timeIntervalSinceNow) < 5, latest.horizontalAccuracy >= 0 else { return }
        if latest.horizontalAccuracy <= 20 {
            if let old = previousFix {
                let dt = latest.timestamp.timeIntervalSince(old.timestamp)
                let distance = latest.distance(from: old)
                // Ignore stale gaps, implausible jumps and sub-accuracy stationary jitter.
                if dt > 0 && dt < 10 && distance/dt < 60 && distance > max(latest.horizontalAccuracy, old.horizontalAccuracy) {
                    trip += distance
                    previousFix = latest
                } else if dt >= 10 { previousFix = latest }
            } else { previousFix = latest }
        }
        fix = latest
        sensorStatus = String(format: "Location accuracy ±%.0f m", latest.horizontalAccuracy)
        publishReadings()
    }
    func saveCamp() {
        guard let f = fix, abs(f.timestamp.timeIntervalSinceNow) < 5, f.horizontalAccuracy >= 0, f.horizontalAccuracy <= 30 else {
            campStatus = "Wait for a fresh location accurate to 30 m"; return
        }
        camp = f
        UserDefaults.standard.set([f.coordinate.latitude, f.coordinate.longitude], forKey: "camp")
        campStatus = "Camp saved on this phone"
    }
    private func packet(_ kind: UInt8, _ values: [Float?]) -> Data {
        var flags: UInt8 = 0
        for (i, value) in values.enumerated() { if let value = value, value.isFinite { flags |= 1 << i } }
        var data = Data([1, kind, flags, 0])
        for value in values {
            var bits = (value ?? 0).bitPattern.littleEndian
            withUnsafeBytes(of: &bits) { data.append(contentsOf: $0) }
        }
        return data
    }
    private func clockPacket() -> Data {
        let now = Date()
        var data = Data([1, 4, 1, 0])
        var milliseconds = UInt64(now.timeIntervalSince1970 * 1000).littleEndian
        var offset = Int32(TimeZone.current.secondsFromGMT(for: now)).littleEndian
        withUnsafeBytes(of: &milliseconds) { data.append(contentsOf: $0) }
        withUnsafeBytes(of: &offset) { data.append(contentsOf: $0) }
        data.append(contentsOf: [0, 0, 0, 0])
        return data
    }
    private func publishReadings() {
        guard running else { return }
        if let fix = fix { weather.refresh(location: fix) }
        weatherStatus = weather.status; cityText = weather.city.isEmpty ? "--" : weather.city
        var gps: [Float?] = [nil,nil,nil,nil]
        var baro: [Float?] = [nil,nil,nil,nil]
        var navigation: [Float?] = [nil,nil,Float(trip),nil]
        if let f = fix, abs(f.timestamp.timeIntervalSinceNow) < 5 {
            if f.verticalAccuracy >= 0 { gps[0] = Float(f.altitude); baro[2] = Float(f.verticalAccuracy) }
            if f.speed >= 0 { gps[1] = Float(f.speed) }
            if f.course >= 0 && f.speed > 0.5 { gps[2] = Float(f.course) }
            if f.horizontalAccuracy >= 0 { gps[3] = Float(f.horizontalAccuracy) }
            if let camp = camp {
                navigation[0] = Float(f.distance(from: camp))
                let lat1 = f.coordinate.latitude * .pi / 180, lat2 = camp.coordinate.latitude * .pi / 180
                let dl = (camp.coordinate.longitude - f.coordinate.longitude) * .pi / 180
                let angle = atan2(sin(dl)*cos(lat2), cos(lat1)*sin(lat2)-sin(lat1)*cos(lat2)*cos(dl)) * 180 / .pi
                navigation[1] = Float((angle + 360).truncatingRemainder(dividingBy: 360))
            }
        }
        if let b = barometer, ProcessInfo.processInfo.systemUptime - b.timestamp < 5 {
            baro[0] = b.pressure.floatValue * 10 // Core Motion kPa -> hPa
            baro[1] = b.relativeAltitude.floatValue
        }
        elevationText = gps[0].map { String(format: "%.0f m", $0) } ?? "--"
        speedText = gps[1].map { String(format: "%.1f km/h", $0*3.6) } ?? "--"
        pressureText = baro[0].map { String(format: "%.1f hPa", $0) } ?? "--"
        pending[1] = packet(1,gps); pending[2] = packet(2,baro); pending[3] = packet(3,navigation); pending[4] = Data()
        for (kind, bytes) in weather.packets() { pending[kind] = bytes }
        sendNext()
    }
    private func scan() {
        guard running, central.state == .poweredOn, peripheral == nil else { return }
        // iOS can retain the physical link across local disconnects/app relaunches.
        // Such a peripheral is not advertising and cannot be found by scanning.
        if let connected = central.retrieveConnectedPeripherals(withServices: [serviceID]).first {
            beginConnection(connected); return
        }
        bluetoothStatus = "Searching for TRAILSTRIP"
        if !central.isScanning { central.scanForPeripherals(withServices: [serviceID]) }
    }
    private func beginConnection(_ p: CBPeripheral) {
        peripheral = p; p.delegate = self; receiver = nil; pending.removeAll(); writing = false
        link.enter(.connecting, now: uptime)
        central.stopScan(); central.connect(p)
        bluetoothStatus = "Connecting"
    }
    private func cancelLink(_ reason: String) {
        receiver = nil; pending.removeAll(); writing = false
        bluetoothStatus = reason
        guard let p = peripheral else { link.enter(.idle, now: uptime); return }
        if link.phase != .cancelling {
            link.enter(.cancelling, now: uptime)
            central.cancelPeripheralConnection(p)
        }
    }
    private func maintainConnection() {
        guard running, central.state == .poweredOn else { return }
        if let p = peripheral, link.phase == .ready, p.state != .connected {
            cancelLink("Link lost; reconnecting")
        }
        switch link.action(now: uptime) {
        case .cancel: cancelLink("Connection stalled; reconnecting")
        case .rebuildManager:
            // Isolate late callbacks from a manager that never completed cancellation.
            central.stopScan(); central.delegate = nil
            if let p = peripheral { p.delegate = nil; central.cancelPeripheralConnection(p) }
            peripheral = nil; receiver = nil; pending.removeAll(); writing = false
            link.enter(.idle, now: uptime)
            bluetoothStatus = "Restarting Bluetooth connection"
            central = CBCentralManager(delegate: self, queue: .main)
        case .none:
            if peripheral == nil { scan() }
        }
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard central === self.central else { return }
        if central.state == .poweredOn { scan() }
        else {
            peripheral?.delegate = nil; peripheral = nil; receiver = nil; pending.removeAll(); writing = false
            link.enter(.idle, now: uptime)
            bluetoothStatus = running ? "Bluetooth unavailable or permission denied" : "Session stopped"
        }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard central === self.central, running, self.peripheral == nil else { return }
        beginConnection(peripheral)
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard central === self.central, peripheral === self.peripheral else { return }
        guard running else { cancelLink("Session stopped"); return }
        guard link.phase == .connecting else { cancelLink("Reconnecting"); return }
        link.enter(.discovering, now: uptime)
        bluetoothStatus = "Discovering TRAILSTRIP service"
        peripheral.discoverServices([serviceID])
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard central === self.central, peripheral === self.peripheral else { return }; disconnected()
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard central === self.central, peripheral === self.peripheral else { return }; disconnected()
    }
    private func disconnected() {
        peripheral?.delegate = nil; peripheral = nil; receiver = nil; writing = false; pending.removeAll()
        link.enter(.idle, now: uptime)
        bluetoothStatus = running ? "Disconnected; reconnecting" : "Session stopped"
        // Retry on the next timer tick, avoiding a tight failure/reconnect loop.
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard peripheral === self.peripheral, running, link.phase == .discovering else { return }
        guard error == nil, let service = peripheral.services?.first(where: { $0.uuid == serviceID }) else {
            cancelLink("Service unavailable; reconnecting"); return
        }
        peripheral.discoverCharacteristics([rxID], for: service)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard peripheral === self.peripheral, running, link.phase == .discovering else { return }
        guard service.uuid == serviceID, error == nil,
              let rx = service.characteristics?.first(where: { $0.uuid == rxID && $0.properties.contains(.write) }) else {
            cancelLink("Receiver unavailable; reconnecting"); return
        }
        receiver = rx; link.enter(.ready, now: uptime)
        bluetoothStatus = "TRAILSTRIP connected"; publishReadings()
    }
    func peripheral(_ peripheral: CBPeripheral, didModifyServices invalidatedServices: [CBService]) {
        guard peripheral === self.peripheral, invalidatedServices.contains(where: { $0.uuid == serviceID }) else { return }
        cancelLink("Service changed; reconnecting")
    }
    private func sendNext() {
        guard running, link.phase == .ready, !writing, let p = peripheral, p.state == .connected, let rx = receiver,
              let kind = pending.keys.sorted().first, let bytes = pending.removeValue(forKey: kind) else { return }
        writing = true; link.beginWrite(now: uptime)
        p.writeValue(kind == 4 ? clockPacket() : bytes, for: rx, type: .withResponse)
    }
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral === self.peripheral, characteristic === receiver, running, link.phase == .ready, writing else { return }
        writing = false; link.finishWrite()
        if let error = error { cancelLink("Write failed; reconnecting: \(error.localizedDescription)"); return }
        sendNext()
    }
}
