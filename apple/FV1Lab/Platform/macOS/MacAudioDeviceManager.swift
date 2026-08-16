#if os(macOS)
import AVFAudio
import AudioToolbox
import Combine
import CoreAudio
import Foundation

struct MacAudioDevice: Identifiable, Hashable, Sendable {
    let id: AudioDeviceID
    let uid: String
    let name: String
    let hasInput: Bool
    let hasOutput: Bool

    var displayName: String {
        name.isEmpty ? uid : name
    }

    var capabilityLabel: String {
        switch (hasInput, hasOutput) {
        case (true, true):
            return "Input + Output"
        case (true, false):
            return "Input"
        case (false, true):
            return "Output"
        case (false, false):
            return "No I/O"
        }
    }
}

enum MacAudioDeviceError: LocalizedError {
    case coreAudio(OSStatus, String)
    case missingAudioUnit(String)
    case deviceUnavailable(String)
    case outputUnavailable(String)
    case inputUnavailable(String)

    var errorDescription: String? {
        switch self {
        case let .coreAudio(status, operation):
            return "\(operation) failed (Core Audio OSStatus \(status))."

        case let .missingAudioUnit(node):
            return "\(node) has no underlying Core Audio AudioUnit."

        case let .deviceUnavailable(uid):
            return "Saved Core Audio device is unavailable: \(uid)"

        case let .outputUnavailable(name):
            return "\(name) does not provide an output stream."

        case let .inputUnavailable(name):
            return """
            \(name) does not provide an input stream. For different physical \
            input and output interfaces, create a macOS Aggregate Device and \
            select that aggregate here.
            """
        }
    }
}

@MainActor
final class MacAudioDeviceManager: ObservableObject {
    @Published private(set) var devices: [MacAudioDevice] = []

    /*
     * One Core Audio I/O device is selected for the AVAudioEngine host.
     * macOS Aggregate Devices are the native way to combine separate physical
     * capture and playback interfaces into this single device abstraction.
     */
    @Published var ioDeviceUID: String {
        didSet {
            defaults.set(
                ioDeviceUID,
                forKey: Keys.ioDeviceUID
            )
        }
    }

    @Published var preferredSampleRate: Double {
        didSet {
            defaults.set(
                preferredSampleRate,
                forKey: Keys.sampleRate
            )
        }
    }

    @Published var preferredBufferFrames: Int {
        didSet {
            defaults.set(
                preferredBufferFrames,
                forKey: Keys.bufferFrames
            )
        }
    }

    private let defaults = UserDefaults.standard

    private enum Keys {
        static let ioDeviceUID =
            "audio/mac/ioDeviceUID"
        static let legacyOutputUID =
            "audio/mac/outputUID"
        static let legacyInputUID =
            "audio/mac/inputUID"
        static let sampleRate =
            "audio/mac/sampleRate"
        static let bufferFrames =
            "audio/mac/bufferFrames"
    }

    init() {
        /*
         * Migrate the Phase-8B/early-8C split-device preference if one exists.
         * Prefer the output choice because every FV-1 Lab session needs output.
         */
        let storedIO =
            defaults.string(
                forKey: Keys.ioDeviceUID
            ) ?? ""

        if !storedIO.isEmpty {
            ioDeviceUID = storedIO
        } else {
            ioDeviceUID =
                defaults.string(
                    forKey: Keys.legacyOutputUID
                )
                ?? defaults.string(
                    forKey: Keys.legacyInputUID
                )
                ?? ""
        }

        let storedRate =
            defaults.double(
                forKey: Keys.sampleRate
            )
        preferredSampleRate =
            storedRate > 0
                ? storedRate
                : 48_000

        let storedBuffer =
            defaults.integer(
                forKey: Keys.bufferFrames
            )
        preferredBufferFrames =
            storedBuffer > 0
                ? storedBuffer
                : 256

        refresh()
    }

    /*
     * All explicitly selectable devices need an output path because FV-1 Lab
     * is an audible testbench. Input-only hardware can still participate by
     * being combined with the output hardware as a macOS Aggregate Device.
     */
    var ioDevices: [MacAudioDevice] {
        devices.filter(\.hasOutput)
    }

    func refresh() {
        devices =
            Self.enumerateDevices()
                .sorted {
                    $0.displayName
                        .localizedCaseInsensitiveCompare(
                            $1.displayName
                        ) == .orderedAscending
                }
    }

    func selectedDevice() -> MacAudioDevice? {
        guard !ioDeviceUID.isEmpty else {
            return nil
        }

        return devices.first {
            $0.uid == ioDeviceUID
        }
    }

    /*
     * Apply an explicit device while AVAudioEngine is stopped.
     *
     * If "OS Default" is selected, AVAudioEngine keeps its normal system route.
     * If a device is explicit, the same AudioDeviceID is assigned to both I/O
     * nodes when live input is required. That is safe for a duplex physical
     * device or a macOS Aggregate Device; we deliberately do not pretend that
     * one AVAudioEngine can independently bind unrelated hardware clocks here.
     */
    func apply(
        to engine: AVAudioEngine,
        needsInput: Bool
    ) throws {
        refresh()

        guard !ioDeviceUID.isEmpty else {
            return
        }

        guard let device = selectedDevice() else {
            throw MacAudioDeviceError
                .deviceUnavailable(
                    ioDeviceUID
                )
        }

        guard device.hasOutput else {
            throw MacAudioDeviceError
                .outputUnavailable(
                    device.displayName
                )
        }

        if needsInput && !device.hasInput {
            throw MacAudioDeviceError
                .inputUnavailable(
                    device.displayName
                )
        }

        try Self.setCurrentDevice(
            device.id,
            on: engine.outputNode,
            label: "output"
        )

        if needsInput {
            /*
             * AVAudioEngine may expose the same AUHAL instance through the two
             * I/O nodes. Re-applying the same AudioDeviceID is harmless and
             * makes the intended duplex/aggregate routing explicit.
             */
            try Self.setCurrentDevice(
                device.id,
                on: engine.inputNode,
                label: "input"
            )
        }

        Self.applyHardwarePreferences(
            deviceID: device.id,
            sampleRate:
                preferredSampleRate,
            bufferFrames:
                preferredBufferFrames
        )
    }

    private static func enumerateDevices()
        -> [MacAudioDevice] {
        var address =
            AudioObjectPropertyAddress(
                mSelector:
                    kAudioHardwarePropertyDevices,
                mScope:
                    kAudioObjectPropertyScopeGlobal,
                mElement: 0
            )

        var byteCount: UInt32 = 0

        guard AudioObjectGetPropertyDataSize(
            AudioObjectID(
                kAudioObjectSystemObject
            ),
            &address,
            0,
            nil,
            &byteCount
        ) == noErr,
        byteCount > 0 else {
            return []
        }

        let count =
            Int(byteCount)
            / MemoryLayout<AudioDeviceID>.size

        var identifiers =
            [AudioDeviceID](
                repeating: 0,
                count: count
            )

        let status: OSStatus =
            identifiers
                .withUnsafeMutableBytes {
                    rawBuffer in

                    /*
                     * Swift 6 correctly models UnsafeMutableRawBufferPointer's
                     * baseAddress as optional. The Core Audio device-property
                     * query already established a non-zero byte count, but
                     * unwrap explicitly instead of force-unwrapping so this
                     * remains safe if the property ever returns an empty
                     * buffer unexpectedly.
                     */
                    guard let baseAddress =
                        rawBuffer.baseAddress else {
                        return kAudio_ParamError
                    }

                    return AudioObjectGetPropertyData(
                        AudioObjectID(
                            kAudioObjectSystemObject
                        ),
                        &address,
                        0,
                        nil,
                        &byteCount,
                        baseAddress
                    )
                }

        guard status == noErr else {
            return []
        }

        return identifiers.compactMap {
            identifier in

            let uid =
                stringProperty(
                    deviceID: identifier,
                    selector:
                        kAudioDevicePropertyDeviceUID
                )

            guard !uid.isEmpty else {
                return nil
            }

            return MacAudioDevice(
                id: identifier,
                uid: uid,
                name:
                    stringProperty(
                        deviceID: identifier,
                        selector:
                            kAudioObjectPropertyName
                    ),
                hasInput:
                    hasStreams(
                        deviceID: identifier,
                        scope:
                            kAudioDevicePropertyScopeInput
                    ),
                hasOutput:
                    hasStreams(
                        deviceID: identifier,
                        scope:
                            kAudioDevicePropertyScopeOutput
                    )
            )
        }
    }

    private static func stringProperty(
        deviceID: AudioDeviceID,
        selector: AudioObjectPropertySelector
    ) -> String {
        var address =
            AudioObjectPropertyAddress(
                mSelector: selector,
                mScope:
                    kAudioObjectPropertyScopeGlobal,
                mElement: 0
            )

        var value: CFString?
        var size =
            UInt32(
                MemoryLayout<CFString?>.size
            )

        let status =
            withUnsafeMutablePointer(
                to: &value
            ) { pointer in
                AudioObjectGetPropertyData(
                    deviceID,
                    &address,
                    0,
                    nil,
                    &size,
                    pointer
                )
            }

        guard status == noErr,
              let value else {
            return ""
        }

        return value as String
    }

    private static func hasStreams(
        deviceID: AudioDeviceID,
        scope: AudioObjectPropertyScope
    ) -> Bool {
        var address =
            AudioObjectPropertyAddress(
                mSelector:
                    kAudioDevicePropertyStreams,
                mScope: scope,
                mElement: 0
            )

        var size: UInt32 = 0

        return AudioObjectGetPropertyDataSize(
            deviceID,
            &address,
            0,
            nil,
            &size
        ) == noErr
            && size >= UInt32(
                MemoryLayout<AudioStreamID>.size
            )
    }

    private static func setCurrentDevice(
        _ deviceID: AudioDeviceID,
        on node: AVAudioIONode,
        label: String
    ) throws {
        guard let audioUnit =
            node.audioUnit else {
            throw MacAudioDeviceError
                .missingAudioUnit(label)
        }

        var mutableID = deviceID

        let status =
            AudioUnitSetProperty(
                audioUnit,
                kAudioOutputUnitProperty_CurrentDevice,
                kAudioUnitScope_Global,
                0,
                &mutableID,
                UInt32(
                    MemoryLayout<AudioDeviceID>.size
                )
            )

        guard status == noErr else {
            throw MacAudioDeviceError
                .coreAudio(
                    status,
                    "Selecting \(label) device"
                )
        }
    }

    /*
     * Device timing preferences are best-effort because externally-clocked or
     * virtual devices may expose these properties read-only.
     */
    private static func applyHardwarePreferences(
        deviceID: AudioDeviceID,
        sampleRate: Double,
        bufferFrames: Int
    ) {
        if sampleRate > 0 {
            var address =
                AudioObjectPropertyAddress(
                    mSelector:
                        kAudioDevicePropertyNominalSampleRate,
                    mScope:
                        kAudioObjectPropertyScopeGlobal,
                    mElement: 0
                )

            var settable: DarwinBoolean = false

            if AudioObjectIsPropertySettable(
                deviceID,
                &address,
                &settable
            ) == noErr,
            settable.boolValue {
                var value = sampleRate
                let size =
                    UInt32(
                        MemoryLayout<Double>.size
                    )

                _ = AudioObjectSetPropertyData(
                    deviceID,
                    &address,
                    0,
                    nil,
                    size,
                    &value
                )
            }
        }

        if bufferFrames > 0 {
            var address =
                AudioObjectPropertyAddress(
                    mSelector:
                        kAudioDevicePropertyBufferFrameSize,
                    mScope:
                        kAudioObjectPropertyScopeGlobal,
                    mElement: 0
                )

            var settable: DarwinBoolean = false

            if AudioObjectIsPropertySettable(
                deviceID,
                &address,
                &settable
            ) == noErr,
            settable.boolValue {
                var value =
                    UInt32(bufferFrames)
                let size =
                    UInt32(
                        MemoryLayout<UInt32>.size
                    )

                _ = AudioObjectSetPropertyData(
                    deviceID,
                    &address,
                    0,
                    nil,
                    size,
                    &value
                )
            }
        }
    }
}
#endif
