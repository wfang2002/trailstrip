import SwiftUI

@main
struct TrailstripApp: App {
    @StateObject private var session = TrailSession()
    var body: some Scene {
        WindowGroup {
            NavigationStack {
                Form {
                    Section("Trail session") {
                        Text(session.bluetoothStatus)
                        Text(session.sensorStatus)
                        Button(session.running ? "End session" : "Start session") {
                            if session.running { session.stop() } else { session.start() }
                        }
                    }
                    Section("Live phone readings") {
                        LabeledContent("Elevation", value: session.elevationText)
                        LabeledContent("Speed", value: session.speedText)
                        LabeledContent("Pressure", value: session.pressureText)
                    }
                    Section("Clock weather") {
                        LabeledContent("City", value: session.cityText)
                        Text(session.weatherStatus)
                        Text("Uses your location for city lookup and local weather. Internet access is required to refresh.")
                        Link("Weather data: Open-Meteo (CC BY 4.0)", destination: URL(string: "https://open-meteo.com/")!)
                    }
                    Section("Camp") {
                        Button("Save current location as camp") { session.saveCamp() }
                            .disabled(!session.running)
                        Text(session.campStatus)
                    }
                    Section {
                        Text("Start a session before driving. Keep the phone with you. The gadget shows dashes when readings expire. Background delivery and reconnection require testing on your iPhone.")
                        Text("Prototype BLE link: one phone at a time, without bonding. Do not use the readings as safety limits.")
                    }
                }
                .navigationTitle("TRAILSTRIP")
                .tint(.orange)
                .preferredColorScheme(.dark)
            }
        }
    }
}
