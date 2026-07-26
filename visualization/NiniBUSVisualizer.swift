import SwiftUI

struct QueueMessage: Identifiable {
    let id = UUID()
    let sequence: UInt64
    let value: String
}

struct CursorState {
    var readSequence: UInt64
    var skippedMessages: UInt64 = 0
}

struct LaneState: Identifiable {
    let id: UInt32
    let capacity: Int
    var size = 0
    var headSequence: UInt64 = 0
    var tailSequence: UInt64 = 0
    var slots: [QueueMessage?]
    var cursors: [UInt32: CursorState] = [:]

    init(id: UInt32, capacity: Int) {
        self.id = id
        self.capacity = capacity
        self.slots = Array(repeating: nil, count: capacity)
    }

    var credit: Int { capacity - size }
}

struct EventItem: Identifiable {
    enum Kind {
        case info, publish, receive, reclaim

        var color: Color {
            switch self {
            case .info: return .secondary
            case .publish: return Color(red: 0.12, green: 0.42, blue: 0.31)
            case .receive: return Color(red: 0.09, green: 0.42, blue: 0.53)
            case .reclaim: return Color(red: 0.64, green: 0.23, blue: 0.20)
            }
        }
    }

    let id = UUID()
    let kind: Kind
    let title: String
    let detail: String
}

@MainActor
final class BusModel: ObservableObject {
    @Published var lanes: [UInt32: LaneState] = [:]
    @Published var selectedLaneID: UInt32 = 10
    @Published var events: [EventItem] = []
    @Published var lastResult = "Ready — publish a message or run the guided demo."
    @Published var lastPublishedMessage = "hello"

    init() {
        reset()
    }

    var laneIDs: [UInt32] { lanes.keys.sorted() }
    var selectedLane: LaneState? { lanes[selectedLaneID] }

    func reset() {
        lanes = [:]
        selectedLaneID = 0
        events = [
            EventItem(
                kind: .info,
                title: "Ready",
                detail: "Call one of the four public APIs. State will appear on the right."
            )
        ]
        lastResult = "Ready — publish a message or run the guided demo."
    }

    func createLane(id: UInt32, capacity: Int) {
        guard capacity > 0 && capacity <= 16 else {
            lastResult = "Invalid capacity — choose 1…16."
            return
        }
        guard lanes[id] == nil else {
            lastResult = "Lane \(id) already exists; createLane() does not replace it."
            return
        }
        lanes[id] = LaneState(id: id, capacity: capacity)
        selectedLaneID = id
        record(.info, "createLane(\(id), \(capacity))", "Created an empty bounded cfifo.")
        lastResult = "CreateLaneStatus::Ok"
    }

    func subscribe(laneID: UInt32, subscriberID: UInt32) {
        guard var lane = lanes[laneID] else {
            lastResult = "SubscribeStatus::LaneNotExist"
            record(.info, "subscribe(\(laneID), \(subscriberID))", "LaneNotExist — subscribe never creates topology.")
            return
        }
        selectedLaneID = laneID
        if let cursor = lane.cursors[subscriberID] {
            lastResult = "SubscribeStatus::Ok — duplicate preserved at sequence \(cursor.readSequence)"
            record(
                .info,
                "subscribe(\(lane.id), \(subscriberID))",
                "Duplicate is idempotent. nextSequenceId remains \(cursor.readSequence)."
            )
        } else {
            lane.cursors[subscriberID] = CursorState(readSequence: lane.tailSequence)
            lanes[laneID] = lane
            lastResult = "SubscribeStatus::Ok — nextSequenceId \(lane.tailSequence)"
            record(
                .info,
                "subscribe(\(lane.id), \(subscriberID))",
                "New cursor starts at tail \(lane.tailSequence); retained history is not replayed."
            )
        }
    }

    func publish(laneID: UInt32, value: String) {
        if lanes[laneID] == nil {
            lanes[laneID] = LaneState(id: laneID, capacity: 10)
            record(
                .info,
                "publish(\(laneID), …)",
                "Missing lane lazily created with DEFAULT_LANE_CAPACITY = 10."
            )
        }
        selectedLaneID = laneID
        guard var lane = lanes[laneID] else { return }
        if lane.size == lane.capacity {
            reclaim(&lane)
        }

        let sequence = lane.tailSequence
        let index = Int(sequence % UInt64(lane.capacity))
        lane.slots[index] = QueueMessage(sequence: sequence, value: value)
        lane.tailSequence += 1
        lane.size += 1
        lanes[laneID] = lane
        lastPublishedMessage = value

        lastResult = "SUCCESS — sequenceId \(sequence), credit \(lane.credit)"
        record(
            .publish,
            "publish(\(lane.id), “\(value)”)",
            "Stored sequence \(sequence) in physical slot \(index). Credit is now \(lane.credit)."
        )
    }

    func receive(laneID: UInt32, subscriberID: UInt32) {
        guard var lane = lanes[laneID] else {
            lastResult = "ReceiveStatus::NO_CURSOR — lane does not exist"
            record(.receive, "receive(\(laneID), \(subscriberID), message)", "NO_CURSOR — receive never creates topology.")
            return
        }
        selectedLaneID = laneID
        guard var cursor = lane.cursors[subscriberID] else {
            lastResult = "ReceiveStatus::NO_CURSOR"
            record(.receive, "receive(\(lane.id), \(subscriberID))", "NO_CURSOR")
            return
        }
        guard cursor.readSequence != lane.tailSequence else {
            lastResult = "ReceiveStatus::NO_PENDING_MESSAGE"
            record(
                .receive,
                "receive(\(lane.id), \(subscriberID))",
                "NO_PENDING_MESSAGE — subscriber is caught up."
            )
            return
        }

        let sequence = cursor.readSequence
        let index = Int(sequence % UInt64(lane.capacity))
        let message = lane.slots[index]?.value ?? "reclaimed"
        cursor.readSequence += 1
        let skipped = cursor.skippedMessages
        cursor.skippedMessages = 0
        lane.cursors[subscriberID] = cursor
        lanes[laneID] = lane

        let pending = lane.tailSequence - cursor.readSequence
        lastResult = "SUCCESS — “\(message)”, pending \(pending), skipped \(skipped)"
        record(
            .receive,
            "receive(\(lane.id), \(subscriberID))",
            "Read sequence \(sequence): “\(message)”. Pending \(pending), skipped \(skipped)."
        )
    }

    func runDemo() {
        reset()
        let operations: [() -> Void] = [
            { self.createLane(id: 10, capacity: 10) },
            { self.subscribe(laneID: 10, subscriberID: 100) },
            { self.subscribe(laneID: 10, subscriberID: 200) },
            { self.publish(laneID: 10, value: "A") },
            { self.publish(laneID: 10, value: "B") },
            { self.receive(laneID: 10, subscriberID: 100) },
            { self.publish(laneID: 10, value: "C") },
            { self.publish(laneID: 10, value: "D") },
            { self.publish(laneID: 10, value: "E") },
            { self.publish(laneID: 10, value: "F") }
        ]

        for (index, operation) in operations.enumerated() {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(index + 1) * 0.65) {
                operation()
            }
        }
    }

    private func reclaim(_ lane: inout LaneState) {
        if lane.cursors.isEmpty {
            lane.headSequence = lane.tailSequence
            lane.size = 0
            record(
                .reclaim,
                "Reclaim: no subscribers",
                "Retained history was discarded before the write."
            )
            return
        }

        let minimum = lane.cursors.values.map(\.readSequence).min() ?? lane.tailSequence
        if minimum == lane.tailSequence {
            lane.headSequence = lane.tailSequence
            lane.size = 0
            record(
                .reclaim,
                "Reclaim: all caught up",
                "Readers had consumed everything. Storage became reusable lazily."
            )
            return
        }

        var affected: [String] = []
        for id in lane.cursors.keys.sorted() {
            guard var cursor = lane.cursors[id], cursor.readSequence == minimum else { continue }
            let skipped = lane.tailSequence - cursor.readSequence
            cursor.skippedMessages += skipped
            cursor.readSequence = lane.tailSequence
            lane.cursors[id] = cursor
            affected.append("\(id) skipped \(skipped)")
        }

        lane.headSequence = lane.cursors.values.map(\.readSequence).min() ?? lane.tailSequence
        lane.size = Int(lane.tailSequence - lane.headSequence)
        record(
            .reclaim,
            "Write pressure → reclaim",
            "\(affected.joined(separator: ", ")). Slowest cursors moved to tail \(lane.tailSequence)."
        )
    }

    private func record(_ kind: EventItem.Kind, _ title: String, _ detail: String) {
        events.insert(EventItem(kind: kind, title: title, detail: detail), at: 0)
        events = Array(events.prefix(40))
    }
}

struct MetricCard: View {
    let label: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(label.uppercased())
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(.secondary)
            Text(value)
                .font(.system(size: 24, weight: .semibold, design: .serif))
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(Color.white)
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.black.opacity(0.10)))
    }
}

struct ArchitectureNode: View {
    let title: String
    let detail: String
    var highlighted = false

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            Text(title)
                .font(.system(size: 14, weight: .bold, design: .monospaced))
            Text(detail)
                .font(.system(size: 12))
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, minHeight: 64, alignment: .leading)
        .padding(14)
        .background(Color.white.opacity(0.92))
        .clipShape(RoundedRectangle(cornerRadius: 14))
        .overlay(
            RoundedRectangle(cornerRadius: 14)
                .stroke(highlighted ? Color.orange : Color.black.opacity(0.12), lineWidth: highlighted ? 2 : 1)
        )
    }
}

struct RingBufferView: View {
    let lane: LaneState
    private let green = Color(red: 0.12, green: 0.42, blue: 0.31)
    private let cursorColors: [Color] = [
        Color(red: 0.09, green: 0.42, blue: 0.53),
        Color(red: 0.48, green: 0.34, blue: 0.65),
        Color(red: 0.63, green: 0.35, blue: 0.14),
        Color(red: 0.23, green: 0.48, blue: 0.22),
        Color(red: 0.64, green: 0.23, blue: 0.20)
    ]

    var body: some View {
        GeometryReader { geometry in
            let center = CGPoint(x: geometry.size.width / 2, y: geometry.size.height / 2)
            let radius = min(geometry.size.width, geometry.size.height) * 0.29
            let retained = Set((0..<lane.size).map { lane.headSequence + UInt64($0) })
            let orderedCursors = lane.cursors.keys.sorted()

            ZStack {
                Circle()
                    .stroke(Color.black.opacity(0.08), lineWidth: 34)
                    .frame(width: radius * 2, height: radius * 2)
                    .position(center)

                VStack(spacing: 5) {
                    Text("cfifo<string>")
                        .font(.system(size: 13, weight: .bold, design: .monospaced))
                    Text("head \(lane.headSequence)  •  tail \(lane.tailSequence)")
                        .font(.system(size: 10, design: .monospaced))
                        .foregroundStyle(.secondary)
                    Text("\(lane.size) / \(lane.capacity)")
                        .font(.system(size: 24, weight: .semibold, design: .serif))
                    Text("credit \(lane.credit)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                    .position(center)

                ForEach(Array(orderedCursors.enumerated()), id: \.element) { offset, subscriberID in
                    if let cursor = lane.cursors[subscriberID] {
                        let slotIndex = Int(cursor.readSequence % UInt64(lane.capacity))
                        let angle = -Double.pi / 2 + (2 * Double.pi * Double(slotIndex) / Double(lane.capacity))
                        let sameSlot = orderedCursors.filter {
                            guard let other = lane.cursors[$0] else { return false }
                            return Int(other.readSequence % UInt64(lane.capacity)) == slotIndex
                        }
                        let collisionIndex = sameSlot.firstIndex(of: subscriberID) ?? 0
                        let markerRadius = radius + 76 + CGFloat(collisionIndex * 34)
                        let markerX = center.x + CGFloat(cos(angle)) * markerRadius
                        let markerY = center.y + CGFloat(sin(angle)) * markerRadius
                        let lineStartRadius = radius + 38
                        let lineEndRadius = markerRadius - 31
                        let lineStart = CGPoint(
                            x: center.x + CGFloat(cos(angle)) * lineStartRadius,
                            y: center.y + CGFloat(sin(angle)) * lineStartRadius
                        )
                        let lineEnd = CGPoint(
                            x: center.x + CGFloat(cos(angle)) * lineEndRadius,
                            y: center.y + CGFloat(sin(angle)) * lineEndRadius
                        )
                        let color = cursorColors[offset % cursorColors.count]
                        let pending = lane.tailSequence - cursor.readSequence
                        let caughtUp = cursor.readSequence == lane.tailSequence

                        Path { path in
                            path.move(to: lineStart)
                            path.addLine(to: lineEnd)
                        }
                        .stroke(
                            cursor.skippedMessages > 0 ? Color.red : color,
                            style: StrokeStyle(lineWidth: 2, dash: cursor.skippedMessages > 0 ? [4, 3] : [])
                        )

                        VStack(spacing: 2) {
                            Text("S\(subscriberID)")
                                .font(.system(size: 10, weight: .black))
                            Text(caughtUp ? "caught up" : "next \(cursor.readSequence)")
                                .font(.system(size: 8, weight: .bold, design: .monospaced))
                            Text("pending \(pending)")
                                .font(.system(size: 8, design: .monospaced))
                            if cursor.skippedMessages > 0 {
                                Text("SKIPPED \(cursor.skippedMessages)")
                                    .font(.system(size: 8, weight: .black))
                                    .foregroundStyle(.red)
                            }
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 6)
                        .foregroundStyle(color)
                        .background(Color.white)
                        .clipShape(RoundedRectangle(cornerRadius: 9))
                        .overlay(
                            RoundedRectangle(cornerRadius: 9)
                                .stroke(cursor.skippedMessages > 0 ? Color.red : color, lineWidth: 2)
                        )
                        .shadow(color: .black.opacity(0.10), radius: 4, y: 2)
                        .position(x: markerX, y: markerY)
                        .animation(.spring(response: 0.45, dampingFraction: 0.78), value: cursor.readSequence)
                    }
                }

                ForEach(0..<lane.capacity, id: \.self) { index in
                    let angle = -Double.pi / 2 + (2 * Double.pi * Double(index) / Double(lane.capacity))
                    let x = center.x + CGFloat(cos(angle)) * radius
                    let y = center.y + CGFloat(sin(angle)) * radius
                    let entry = lane.slots[index]
                    let isRetained = entry.map { retained.contains($0.sequence) } ?? false
                    let isNextWrite = Int(lane.tailSequence % UInt64(lane.capacity)) == index

                    VStack(spacing: 2) {
                        Text("[\(index)]")
                            .font(.system(size: 8, design: .monospaced))
                            .foregroundStyle(.secondary)
                        Text(isRetained ? (entry?.value ?? "") : "—")
                            .font(.system(size: 10, weight: .bold))
                            .lineLimit(1)
                        Text(isRetained ? "seq \(entry?.sequence ?? 0)" : "empty")
                            .font(.system(size: 8, weight: .semibold, design: .monospaced))
                            .foregroundStyle(isRetained ? .blue : .secondary)
                        if isNextWrite {
                            Text("NEXT")
                                .font(.system(size: 7, weight: .black))
                                .foregroundStyle(.orange)
                        }
                    }
                    .frame(width: 72, height: 58)
                    .background(isRetained ? green.opacity(0.18) : Color.white)
                    .clipShape(RoundedRectangle(cornerRadius: 9))
                    .overlay(
                        RoundedRectangle(cornerRadius: 9)
                            .stroke(isNextWrite ? Color.orange : (isRetained ? green : Color.black.opacity(0.16)), lineWidth: isNextWrite ? 2 : 1)
                    )
                    .position(x: x, y: y)
                }
            }
        }
        .frame(minHeight: 500)
    }
}

struct ContentView: View {
    @StateObject private var model = BusModel()
    @State private var createLaneID = "10"
    @State private var createCapacity = "10"
    @State private var publishLaneID = "10"
    @State private var publishMessage = "temperature=24"
    @State private var subscribeLaneID = "10"
    @State private var subscribeSubscriberID = "100"
    @State private var receiveLaneID = "10"
    @State private var receiveSubscriberID = "100"

    private let green = Color(red: 0.12, green: 0.42, blue: 0.31)
    private let paper = Color(red: 0.96, green: 0.95, blue: 0.91)

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            ScrollView {
                VStack(spacing: 16) {
                    HStack(alignment: .top, spacing: 16) {
                        apiConsole.frame(width: 350)
                        visualizationArea.frame(minWidth: 810)
                    }
                }
                .padding(18)
            }
        }
        .frame(minWidth: 1220, minHeight: 760)
        .background(paper)
    }

    private var header: some View {
        HStack(alignment: .center, spacing: 20) {
            VStack(alignment: .leading, spacing: 4) {
                Text("INTERACTIVE SYSTEMS EXPLAINER")
                    .font(.system(size: 10, weight: .bold))
                    .tracking(1.6)
                    .foregroundStyle(.orange)
                Text("See niniBUS move.")
                    .font(.system(size: 34, weight: .semibold, design: .serif))
                Text("Publish strings, move cursors, and watch the write-first policy reclaim storage.")
                    .font(.system(size: 13))
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Button("Reset") { model.reset() }
            Button("Run guided demo") { model.runDemo() }
                .buttonStyle(.borderedProminent)
                .tint(.orange)
        }
        .padding(.horizontal, 22)
        .padding(.vertical, 14)
        .background(Color.white.opacity(0.82))
    }

    private var apiConsole: some View {
        VStack(spacing: 14) {
            Panel(title: "Public API", subtitle: "enter arguments, then call") {
                VStack(spacing: 14) {
                    apiCard(
                        number: "01",
                        signature: "createLane(laneID, capacity)",
                        note: "Explicitly adds an entry to lanes_."
                    ) {
                        HStack {
                            TextField("laneID", text: $createLaneID)
                            TextField("capacity", text: $createCapacity)
                        }
                        Button("Call createLane()") {
                            guard let id = UInt32(createLaneID), let capacity = Int(createCapacity) else { return }
                            model.createLane(id: id, capacity: capacity)
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(.orange)
                    }

                    apiCard(
                        number: "02",
                        signature: "publish(laneID, message)",
                        note: "Finds the Lane and calls content_.write(). Missing lanes use default capacity."
                    ) {
                        TextField("laneID", text: $publishLaneID)
                        TextField("message", text: $publishMessage)
                        Button("Call publish()") {
                            guard let id = UInt32(publishLaneID) else { return }
                            model.publish(laneID: id, value: publishMessage)
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(green)
                    }

                    apiCard(
                        number: "03",
                        signature: "subscribe(laneID, subscriberID)",
                        note: "Creates a cursor at the lane's current tail."
                    ) {
                        HStack {
                            TextField("laneID", text: $subscribeLaneID)
                            TextField("subscriberID", text: $subscribeSubscriberID)
                        }
                        Button("Call subscribe()") {
                            guard
                                let laneID = UInt32(subscribeLaneID),
                                let subscriberID = UInt32(subscribeSubscriberID)
                            else { return }
                            model.subscribe(laneID: laneID, subscriberID: subscriberID)
                        }
                        .buttonStyle(.bordered)
                    }

                    apiCard(
                        number: "04",
                        signature: "receive(laneID, subscriberID, message&)",
                        note: "Reads at that subscriber's cursor and advances only that cursor."
                    ) {
                        HStack {
                            TextField("laneID", text: $receiveLaneID)
                            TextField("subscriberID", text: $receiveSubscriberID)
                        }
                        Button("Call receive()") {
                            guard
                                let laneID = UInt32(receiveLaneID),
                                let subscriberID = UInt32(receiveSubscriberID)
                            else { return }
                            model.receive(laneID: laneID, subscriberID: subscriberID)
                        }
                        .buttonStyle(.bordered)
                    }
                }
                .textFieldStyle(.roundedBorder)
            }

            Panel(title: "Return value", subtitle: "latest API result") {
                Text(model.lastResult)
                    .font(.system(size: 11, weight: .semibold, design: .monospaced))
                    .foregroundStyle(green)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }

    private func apiCard<Content: View>(
        number: String,
        signature: String,
        note: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack(alignment: .firstTextBaseline) {
                Text(number)
                    .font(.system(size: 9, weight: .black))
                    .foregroundStyle(.orange)
                Text(signature)
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
            }
            Text(note)
                .font(.system(size: 10))
                .foregroundStyle(.secondary)
            content()
        }
        .padding(11)
        .background(Color.white)
        .clipShape(RoundedRectangle(cornerRadius: 11))
        .overlay(RoundedRectangle(cornerRadius: 11).stroke(Color.black.opacity(0.10)))
    }

    private var visualizationArea: some View {
        VStack(spacing: 16) {
            lanesTable
            if let lane = model.selectedLane {
                HStack(alignment: .top, spacing: 16) {
                    laneObjectPanel(lane)
                        .frame(width: 255)
                    Panel(
                        title: "content_ → nbus::cfifo<std::string>",
                        subtitle: "physical slots + sequence IDs + subscriber markers"
                    ) {
                        VStack(spacing: 8) {
                            RingBufferView(lane: lane)
                            HStack(spacing: 16) {
                                Label("Marker points to next read", systemImage: "person.crop.circle")
                                Label("receive() moves one marker", systemImage: "arrow.forward.circle")
                                Label("reclaim jumps slow markers to tail", systemImage: "exclamationmark.arrow.triangle.2.circlepath")
                            }
                            .font(.system(size: 9, weight: .semibold))
                            .foregroundStyle(.secondary)
                        }
                    }
                }
                HStack(alignment: .top, spacing: 16) {
                    subscriberPanel
                    eventPanel
                }
            } else {
                VStack(spacing: 12) {
                    Image(systemName: "arrow.left.circle")
                        .font(.system(size: 38))
                        .foregroundStyle(.orange)
                    Text("Call createLane() or publish()")
                        .font(.system(size: 18, weight: .semibold, design: .serif))
                    Text("The lanes_ table, Lane object, and cfifo ring will be constructed here.")
                        .font(.system(size: 12))
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, minHeight: 420)
                .background(Color.white.opacity(0.55))
                .clipShape(RoundedRectangle(cornerRadius: 16))
                .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.black.opacity(0.10), style: StrokeStyle(dash: [6])))
            }
        }
    }

    private var lanesTable: some View {
        Panel(title: "lanes_ table", subtitle: "unordered_map<laneID_t, Lane>") {
            VStack(spacing: 0) {
                HStack(spacing: 0) {
                    tableCell("laneID", width: 90, header: true)
                    Divider()
                    tableCell("Lane object", width: 170, header: true)
                    Divider()
                    tableCell("cfifo state", width: 245, header: true)
                    Divider()
                    tableCell("subscribers", width: 115, header: true)
                    Spacer()
                }
                .frame(height: 30)
                Divider()

                if model.laneIDs.isEmpty {
                    Text("empty — no lane objects exist")
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity, minHeight: 42, alignment: .leading)
                        .padding(.horizontal, 10)
                } else {
                    ForEach(model.laneIDs, id: \.self) { id in
                        if let lane = model.lanes[id] {
                            Button {
                                model.selectedLaneID = id
                            } label: {
                                HStack(spacing: 0) {
                                    tableCell("\(lane.id)", width: 90, accent: true)
                                    Divider()
                                    tableCell("Lane@\(lane.id)", width: 170)
                                    Divider()
                                    VStack(alignment: .leading, spacing: 4) {
                                        occupancyBar(lane: lane, compact: true)
                                        Text("\(lane.size)/\(lane.capacity) • credit \(lane.credit)")
                                            .font(.system(size: 8, weight: .bold, design: .monospaced))
                                            .foregroundStyle(.secondary)
                                    }
                                    .frame(width: 245, alignment: .leading)
                                    .padding(.horizontal, 8)
                                    Divider()
                                    tableCell("\(lane.cursors.count)", width: 115)
                                    Spacer()
                                }
                                .frame(height: 48)
                                .background(id == model.selectedLaneID ? Color.orange.opacity(0.10) : Color.clear)
                            }
                            .buttonStyle(.plain)
                            Divider()
                        }
                    }
                }
            }
        }
    }

    private func tableCell(
        _ value: String,
        width: CGFloat,
        header: Bool = false,
        accent: Bool = false
    ) -> some View {
        Text(value)
            .font(.system(size: header ? 9 : 11, weight: header || accent ? .bold : .regular, design: .monospaced))
            .foregroundStyle(accent ? .orange : (header ? .secondary : .primary))
            .frame(width: width, alignment: .leading)
            .padding(.horizontal, 9)
    }

    private func laneObjectPanel(_ lane: LaneState) -> some View {
        Panel(title: "Lane object", subtitle: "mapped value for key \(lane.id)") {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("Lane@\(lane.id)")
                        .font(.system(size: 16, weight: .bold, design: .monospaced))
                    Spacer()
                    Text("laneID \(lane.id)")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                }
                Divider()
                objectMember(name: "content_", value: "cfifo<string>")
                objectMember(name: "capacity", value: "\(lane.capacity)")
                objectMember(name: "size", value: "\(lane.size)")
                objectMember(name: "credit", value: "\(lane.credit)")
                objectMember(name: "head_sequence_", value: "\(lane.headSequence)")
                objectMember(name: "tail_sequence_", value: "\(lane.tailSequence)")
                Divider()
                Text("The Lane owns the queue. niniBUS only looks up the lane and delegates.")
                    .font(.system(size: 10))
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func objectMember(name: String, value: String) -> some View {
        HStack {
            Text(name)
                .font(.system(size: 10, design: .monospaced))
                .foregroundStyle(.secondary)
            Spacer()
            Text(value)
                .font(.system(size: 10, weight: .bold, design: .monospaced))
        }
    }

    private func occupancyBar(lane: LaneState, compact: Bool = false) -> some View {
        HStack(spacing: compact ? 3 : 5) {
            ForEach(0..<lane.capacity, id: \.self) { index in
                RoundedRectangle(cornerRadius: compact ? 2 : 4)
                    .fill(index < lane.size ? green : Color.black.opacity(0.10))
                    .frame(
                        minWidth: compact ? 10 : 16,
                        maxWidth: compact ? 18 : .infinity,
                        minHeight: compact ? 18 : 28,
                        maxHeight: compact ? 18 : 28
                    )
            }
        }
        .animation(.easeInOut(duration: 0.2), value: lane.size)
    }

    private var subscriberPanel: some View {
        Panel(title: "Subscriber cursors", subtitle: "next unread sequence") {
            if let lane = model.selectedLane, !lane.cursors.isEmpty {
                VStack(spacing: 9) {
                    ForEach(Array(lane.cursors.keys.sorted().enumerated()), id: \.element) { offset, id in
                        if let cursor = lane.cursors[id] {
                            let pending = lane.tailSequence - cursor.readSequence
                            let markerColors: [Color] = [.blue, .purple, .brown, .green, .red]
                            let markerColor = markerColors[offset % markerColors.count]
                            VStack(alignment: .leading, spacing: 6) {
                                HStack {
                                    Circle()
                                        .fill(markerColor)
                                        .frame(width: 9, height: 9)
                                    Text("subscriber \(id)").fontWeight(.bold)
                                    Spacer()
                                    Text("next \(cursor.readSequence)")
                                        .font(.system(size: 10, design: .monospaced))
                                }
                                ProgressView(value: Double(pending), total: Double(max(1, lane.capacity)))
                                    .tint(.orange)
                                HStack {
                                    Text("pending \(pending)")
                                    Spacer()
                                    Text(
                                        cursor.skippedMessages > 0
                                            ? "⚠ skipped \(cursor.skippedMessages)"
                                            : "skipped 0"
                                    )
                                    .foregroundStyle(cursor.skippedMessages > 0 ? .red : .secondary)
                                }
                                .font(.caption2)
                            }
                            .padding(10)
                            .background(Color.white)
                            .clipShape(RoundedRectangle(cornerRadius: 10))
                            .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.black.opacity(0.10)))
                        }
                    }
                }
            } else {
                Text("No cursors. Publishing still works; a full write reclaims all retained history.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var eventPanel: some View {
        Panel(title: "Operation log", subtitle: "newest first") {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 8) {
                    ForEach(model.events) { event in
                        HStack(alignment: .top, spacing: 8) {
                            Rectangle()
                                .fill(event.kind.color)
                                .frame(width: 3)
                            VStack(alignment: .leading, spacing: 3) {
                                Text(event.title)
                                    .font(.system(size: 11, weight: .bold))
                                Text(event.detail)
                                    .font(.system(size: 10))
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(event.kind.color.opacity(0.07))
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                    }
                }
            }
            .frame(maxHeight: 310)
        }
    }
}

struct Panel<Content: View>: View {
    let title: String
    let subtitle: String
    @ViewBuilder let content: Content

    init(title: String, subtitle: String, @ViewBuilder content: () -> Content) {
        self.title = title
        self.subtitle = subtitle
        self.content = content()
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text(title).font(.system(size: 13, weight: .bold))
                Spacer()
                Text(subtitle).font(.system(size: 10)).foregroundStyle(.secondary)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 11)
            Divider()
            content.padding(14)
        }
        .background(Color(red: 1.0, green: 0.995, blue: 0.97))
        .clipShape(RoundedRectangle(cornerRadius: 15))
        .overlay(RoundedRectangle(cornerRadius: 15).stroke(Color.black.opacity(0.10)))
        .shadow(color: .black.opacity(0.06), radius: 14, y: 6)
    }
}

@main
struct NiniBUSVisualizerApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .windowStyle(.titleBar)
        .windowResizability(.contentSize)
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
