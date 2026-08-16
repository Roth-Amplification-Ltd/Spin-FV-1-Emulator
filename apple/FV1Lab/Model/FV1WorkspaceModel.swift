import Combine
import Foundation

@MainActor
final class FV1WorkspaceModel: ObservableObject {
    @Published var pot0: Double = 0 {
        didSet { syncPots() }
    }
    @Published var pot1: Double = 0 {
        didSet { syncPots() }
    }
    @Published var pot2: Double = 0 {
        didSet { syncPots() }
    }

    @Published var debugInputLeft: Double = 0
    @Published var debugInputRight: Double = 0

    @Published private(set) var compileSummary =
        "Ready"
    @Published private(set) var console =
        "FV-1 Lab Apple frontend ready.\n"

    @Published private(set) var snapshot:
        FV1ChipSnapshot?
    @Published private(set) var resources:
        FV1ResourceSummary?
    @Published private(set) var loadedProgram:
        Data?
    @Published private(set) var debugTrace:
        FV1DebugTrace?

    @Published private(set) var delayWords:
        [FV1DelayWord] = []
    @Published var delayCenterAddress:
        UInt32 = 0

    var audio = AppleAudioController()

    private var audioObservation:
        AnyCancellable?
    private let inspection: FV1Engine

    init() {
        do {
            inspection = try FV1Engine()
        } catch {
            fatalError(
                "FV-1 inspection engine could not be created: \(error)"
            )
        }

        audioObservation =
            audio.objectWillChange
                .sink { [weak self] _ in
                    self?.objectWillChange
                        .send()
                }
    }

    func compileAndLoad(
        source: String
    ) {
        do {
            let compiled =
                try FV1Engine.compile(
                    source
                )

            try load(
                program:
                    compiled.program
            )

            compileSummary =
                "Loaded \(compiled.instructionCount) instructions · delay high \(compiled.highestDelayAddress)"

            append(
                "Compile/load OK: \(compiled.instructionCount) instructions"
            )
        } catch {
            compileSummary =
                "Compile/load failed"
            append(
                error.localizedDescription
            )
        }
    }

    func load(
        program: Data
    ) throws {
        try inspection.load(
            program: program
        )
        try inspection.setPots(
            currentPots
        )

        try audio.setProgram(
            program
        )

        loadedProgram = program
        debugTrace = nil
        delayCenterAddress = 0
        refreshInspection()
    }

    func loadRawProgram(
        _ data: Data
    ) {
        do {
            try load(
                program: data
            )

            compileSummary =
                "Loaded raw 512-byte program"

            append(
                "Raw FV-1 program loaded"
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func reset() {
        do {
            try inspection.reset()
            try inspection.setPots(
                currentPots
            )

            try audio.resetChip()

            debugTrace = nil
            delayCenterAddress = 0
            refreshInspection()

            append(
                "Virtual FV-1 reset"
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func refreshInspection() {
        do {
            let newSnapshot =
                try inspection.snapshot()
            snapshot = newSnapshot

            resources =
                try inspection.resources()

            if delayWords.isEmpty {
                delayCenterAddress =
                    newSnapshot.delayPointer
            }

            try refreshDelayWindow(
                center:
                    delayCenterAddress
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func stepInstruction() {
        guard loadedProgram != nil else {
            append(
                "Load a program before stepping the offline chip inspector."
            )
            return
        }

        do {
            debugTrace =
                try inspection
                    .debugStepInstruction(
                        inputLeft:
                            Float(debugInputLeft),
                        inputRight:
                            Float(debugInputRight)
                    )

            let newSnapshot =
                try inspection.snapshot()
            snapshot = newSnapshot
            delayCenterAddress =
                newSnapshot.delayPointer

            try refreshDelayWindow(
                center:
                    newSnapshot.delayPointer
            )

            if let trace = debugTrace {
                append(
                    "Inspector step: sample \(trace.sampleIndex) instruction \(trace.instructionIndex) PC \(trace.pcBefore)→\(trace.pcAfter) \(trace.opcodeName)"
                )
            }
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func stepSample() {
        guard loadedProgram != nil else {
            append(
                "Load a program before stepping the offline chip inspector."
            )
            return
        }

        do {
            debugTrace =
                try inspection
                    .debugStepSample(
                        inputLeft:
                            Float(debugInputLeft),
                        inputRight:
                            Float(debugInputRight)
                    )

            let newSnapshot =
                try inspection.snapshot()
            snapshot = newSnapshot
            delayCenterAddress =
                newSnapshot.delayPointer

            try refreshDelayWindow(
                center:
                    newSnapshot.delayPointer
            )

            append(
                "Inspector completed virtual sample \(newSnapshot.sampleCounter)."
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func centerDelayOnPointer() {
        guard let snapshot else {
            return
        }

        delayCenterAddress =
            snapshot.delayPointer

        do {
            try refreshDelayWindow(
                center:
                    snapshot.delayPointer
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func jumpDelayWindow(
        to address: UInt32
    ) {
        let bounded =
            address
            % UInt32(
                FV1_SDK_DELAY_WORDS
            )

        delayCenterAddress = bounded

        do {
            try refreshDelayWindow(
                center: bounded
            )
        } catch {
            append(
                error.localizedDescription
            )
        }
    }

    func registerName(
        _ index: Int
    ) -> String {
        inspection.registerName(
            index: UInt32(index)
        )
    }

    func reportExternalError(
        _ message: String
    ) {
        append(message)
    }

    func reportStatus(
        _ message: String
    ) {
        append(message)
    }

    private var currentPots:
        [Float] {
        [
            Float(pot0),
            Float(pot1),
            Float(pot2)
        ]
    }

    private func syncPots() {
        let pots =
            currentPots

        do {
            try inspection.setPots(
                pots
            )
        } catch {
            append(
                error.localizedDescription
            )
        }

        audio.setPots(
            pots
        )
    }

    private func refreshDelayWindow(
        center: UInt32
    ) throws {
        delayWords =
            try inspection
                .delayWindow(
                    centeredAt: center,
                    count: 96
                )
    }

    private func append(
        _ message: String
    ) {
        console +=
            "\(message)\n"

        if console.count > 20_000 {
            console.removeFirst(
                console.count
                    - 20_000
            )
        }
    }
}
