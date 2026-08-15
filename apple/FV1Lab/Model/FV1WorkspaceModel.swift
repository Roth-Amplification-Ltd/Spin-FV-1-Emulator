import Combine
import Foundation

@MainActor
final class FV1WorkspaceModel: ObservableObject {
    @Published var pot0: Double = 0 { didSet { syncPots() } }
    @Published var pot1: Double = 0 { didSet { syncPots() } }
    @Published var pot2: Double = 0 { didSet { syncPots() } }
    @Published private(set) var compileSummary = "Ready"
    @Published private(set) var console = "FV-1 Lab Apple frontend ready.\n"
    @Published private(set) var snapshot: FV1ChipSnapshot?
    @Published private(set) var resources: FV1ResourceSummary?
    @Published private(set) var loadedProgram: Data?

    // Mutable so SwiftUI can form writable bindings to the nested
    // AppleAudioController properties used by the source/generator controls.
    //
    // FV1WorkspaceModel remains the root ObservableObject for WorkspaceView,
    // so forward the nested controller's objectWillChange notifications as
    // well. This keeps Start/Stop state, telemetry, source selection, scope
    // state, route information, and generator controls live in the dashboard.
    var audio = AppleAudioController()

    private var audioObservation: AnyCancellable?
    private let inspection: FV1Engine

    init() {
        do {
            inspection = try FV1Engine()
        } catch {
            fatalError(
                "FV-1 inspection engine could not be created: \(error)"
            )
        }

        audioObservation = audio.objectWillChange.sink { [weak self] _ in
            self?.objectWillChange.send()
        }
    }

    func compileAndLoad(source: String) {
        do {
            let compiled = try FV1Engine.compile(source)
            try load(program: compiled.program)
            compileSummary = "Loaded \(compiled.instructionCount) instructions · delay high \(compiled.highestDelayAddress)"
            append("Compile/load OK: \(compiled.instructionCount) instructions")
        } catch {
            compileSummary = "Compile/load failed"
            append(error.localizedDescription)
        }
    }

    func load(program: Data) throws {
        try inspection.load(program: program)
        try inspection.setPots(currentPots)
        try audio.setProgram(program)
        loadedProgram = program
        refreshInspection()
    }

    func loadRawProgram(_ data: Data) {
        do {
            try load(program: data)
            compileSummary = "Loaded raw 512-byte program"
            append("Raw FV-1 program loaded")
        } catch { append(error.localizedDescription) }
    }

    func reset() {
        do {
            try inspection.reset()
            try inspection.setPots(currentPots)
            try audio.resetChip()
            refreshInspection()
            append("Virtual FV-1 reset")
        } catch { append(error.localizedDescription) }
    }

    func refreshInspection() {
        do {
            snapshot = try inspection.snapshot()
            resources = try inspection.resources()
        } catch { append(error.localizedDescription) }
    }

    func reportExternalError(_ message: String) {
        append(message)
    }

    private var currentPots: [Float] { [Float(pot0), Float(pot1), Float(pot2)] }

    private func syncPots() {
        let pots = currentPots
        do { try inspection.setPots(pots) } catch { append(error.localizedDescription) }
        audio.setPots(pots)
    }

    private func append(_ message: String) {
        console += "\(message)\n"
        if console.count > 20_000 { console.removeFirst(console.count - 20_000) }
    }
}
