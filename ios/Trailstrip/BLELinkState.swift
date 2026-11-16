import Foundation

// Monotonic deadlines; shared by the real CoreBluetooth path and host tests.
struct BLELinkState {
    enum Phase { case idle, connecting, discovering, ready, cancelling }
    enum Action { case none, cancel, rebuildManager }
    private(set) var phase: Phase = .idle
    private var enteredAt: TimeInterval = 0
    private var writeStartedAt: TimeInterval?
    mutating func enter(_ phase: Phase, now: TimeInterval) {
        self.phase = phase; enteredAt = now; writeStartedAt = nil
    }
    mutating func beginWrite(now: TimeInterval) { writeStartedAt = now }
    mutating func finishWrite() { writeStartedAt = nil }
    func action(now: TimeInterval) -> Action {
        switch phase {
        case .idle: return .none
        case .connecting: return now-enteredAt >= 15 ? .cancel : .none
        case .discovering: return now-enteredAt >= 10 ? .cancel : .none
        case .cancelling: return now-enteredAt >= 8 ? .rebuildManager : .none
        case .ready: return writeStartedAt.map { now-$0 >= 8 ? .cancel : .none } ?? .none
        }
    }
}
