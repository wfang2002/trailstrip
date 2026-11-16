import Foundation
@main struct LinkTests {
    static func main() {
        var link = BLELinkState()
        assert(link.action(now: 100) == .none)
        link.enter(.connecting, now: 100)
        assert(link.action(now: 114.999) == .none)
        assert(link.action(now: 115) == .cancel)
        link.enter(.cancelling, now: 115)
        assert(link.action(now: 122.999) == .none)
        assert(link.action(now: 123) == .rebuildManager)
        link.enter(.discovering, now: 200)
        assert(link.action(now: 210) == .cancel)
        link.enter(.ready, now: 210)
        assert(link.action(now: 9999) == .none)
        link.beginWrite(now: 220)
        assert(link.action(now: 227.999) == .none)
        assert(link.action(now: 228) == .cancel)
        link.finishWrite();assert(link.action(now: 229) == .none)
        for i in 0..<50 {
            let t = Double(i*100)
            link.enter(.connecting, now: t);link.enter(.discovering, now: t+1)
            link.enter(.ready, now: t+2);link.beginWrite(now: t+3);link.finishWrite()
            assert(link.action(now: t+4) == .none)
            link.enter(.cancelling, now: t+5);link.enter(.idle, now: t+6)
            assert(link.action(now: t+50) == .none)
        }
        print("iOS BLE deadlines: connect/discovery/write stalls, missing cancellation callback and 50 stop/start cycles passed")
    }
}
