import Foundation
import SwiftUI

struct FV1CompileActionKey:
    FocusedValueKey {
    typealias Value = () -> Void
}

extension FocusedValues {
    var fv1CompileAction:
        (() -> Void)? {
        get {
            self[
                FV1CompileActionKey.self
            ]
        }
        set {
            self[
                FV1CompileActionKey.self
            ] = newValue
        }
    }
}

#if os(macOS)
struct FV1OpenProgramActionKey:
    FocusedValueKey {
    typealias Value = () -> Void
}

struct FV1PasteSpinASMActionKey:
    FocusedValueKey {
    typealias Value = () -> Void
}

struct FV1ToggleDSPActionKey:
    FocusedValueKey {
    typealias Value = () -> Void
}

struct FV1RecordActionKey:
    FocusedValueKey {
    typealias Value =
        (AppleRecordMode) -> Void
}

struct FV1StopRecordActionKey:
    FocusedValueKey {
    typealias Value = () -> Void
}

struct FV1RecordingStateKey:
    FocusedValueKey {
    typealias Value = Bool
}

extension FocusedValues {
    var fv1OpenProgramAction:
        (() -> Void)? {
        get {
            self[
                FV1OpenProgramActionKey.self
            ]
        }
        set {
            self[
                FV1OpenProgramActionKey.self
            ] = newValue
        }
    }

    var fv1PasteSpinASMAction:
        (() -> Void)? {
        get {
            self[
                FV1PasteSpinASMActionKey.self
            ]
        }
        set {
            self[
                FV1PasteSpinASMActionKey.self
            ] = newValue
        }
    }

    var fv1ToggleDSPAction:
        (() -> Void)? {
        get {
            self[
                FV1ToggleDSPActionKey.self
            ]
        }
        set {
            self[
                FV1ToggleDSPActionKey.self
            ] = newValue
        }
    }

    var fv1RecordAction:
        ((AppleRecordMode) -> Void)? {
        get {
            self[
                FV1RecordActionKey.self
            ]
        }
        set {
            self[
                FV1RecordActionKey.self
            ] = newValue
        }
    }

    var fv1StopRecordAction:
        (() -> Void)? {
        get {
            self[
                FV1StopRecordActionKey.self
            ]
        }
        set {
            self[
                FV1StopRecordActionKey.self
            ] = newValue
        }
    }

    var fv1RecordingState:
        Bool? {
        get {
            self[
                FV1RecordingStateKey.self
            ]
        }
        set {
            self[
                FV1RecordingStateKey.self
            ] = newValue
        }
    }
}
#endif

struct FV1Commands: Commands {
    @FocusedValue(
        \.fv1CompileAction
    )
    private var compileAction

    #if os(macOS)
    @FocusedValue(
        \.fv1OpenProgramAction
    )
    private var openProgramAction

    @FocusedValue(
        \.fv1PasteSpinASMAction
    )
    private var pasteSpinASMAction

    @FocusedValue(
        \.fv1ToggleDSPAction
    )
    private var toggleDSPAction

    @FocusedValue(
        \.fv1RecordAction
    )
    private var recordAction

    @FocusedValue(
        \.fv1StopRecordAction
    )
    private var stopRecordAction

    @FocusedValue(
        \.fv1RecordingState
    )
    private var recordingState

    @AppStorage(
        "analysis/fftSize"
    )
    private var analyzerFFTSize =
        4096

    @AppStorage(
        "appearance/theme"
    )
    private var themeName =
        "Dark"

    @AppStorage(
        "appearance/accent"
    )
    private var accentName =
        "Cyan"

    @AppStorage(
        "appearance/icon"
    )
    private var applicationIconName =
        "Silver"
    #endif

    var body: some Commands {
        #if os(macOS)
        CommandGroup(
            replacing: .appInfo
        ) {
            Button(
                "About FV-1 Lab"
            ) {
                MacAboutPresenter.show()
            }
        }

        /*
         * File menu mirrors the Linux desktop application while retaining
         * normal macOS Quit/Close behavior supplied by SwiftUI/AppKit.
         */
        CommandGroup(
            replacing: .newItem
        ) {
            Button(
                "Open FV-1 Program…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1OpenProgram,
                        object: nil
                    )
            }
            .keyboardShortcut("o")

            Button(
                "Paste SpinASM…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1PasteSpinASM,
                        object: nil
                    )
            }
            .keyboardShortcut(
                "v",
                modifiers: [
                    .command,
                    .shift
                ]
            )

            Button(
                "Open Audio Loop…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1OpenAudioLoop,
                        object: nil
                    )
            }
        }

        CommandMenu("Audio") {
            Button(
                "Audio Settings…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1ShowAudioSettings,
                        object: nil
                    )
            }
            .keyboardShortcut(",")

            Button(
                "Refresh Audio Devices"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1RefreshAudioDevices,
                        object: nil
                    )
            }

            Divider()

            Button(
                "Test Generator Settings…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1ShowGeneratorSettings,
                        object: nil
                    )
            }

            Button(
                "Audio Loop Region…"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1ShowAudioLoopRegion,
                        object: nil
                    )
            }

            Divider()

            Menu(
                "Record Raw / Processed Audio…"
            ) {
                Button("Processed") {
                    recordAction?(
                        .processed
                    )
                }
                .disabled(
                    recordAction == nil
                )

                Button("Raw") {
                    recordAction?(
                        .raw
                    )
                }
                .disabled(
                    recordAction == nil
                )

                Button(
                    "Raw + Processed"
                ) {
                    recordAction?(
                        .rawAndProcessed
                    )
                }
                .disabled(
                    recordAction == nil
                )
            }

            Button(
                "Stop Recording"
            ) {
                stopRecordAction?()
            }
            .disabled(
                stopRecordAction == nil
                    || recordingState
                        != true
            )

            Divider()

            Button(
                "Toggle DSP/FX Bypass"
            ) {
                toggleDSPAction?()
            }
            .keyboardShortcut(
                "d",
                modifiers: [
                    .command,
                    .shift
                ]
            )
            .disabled(
                toggleDSPAction == nil
            )
        }

        CommandMenu("Analysis") {
            Button(
                "Show Raw + Processed Overlay"
            ) {
                let defaults =
                    UserDefaults.standard
                let key =
                    "analysis/rawProcessedOverlay"

                let current =
                    defaults.object(
                        forKey: key
                    ) == nil
                    ? true
                    : defaults.bool(
                        forKey: key
                    )

                let enabled =
                    !current

                defaults.set(
                    enabled,
                    forKey: key
                )

                NotificationCenter.default
                    .post(
                        name:
                            .fv1AnalyzerOverlayChanged,
                        object: enabled
                    )
            }

            Divider()

            Button(
                "Freeze / Unfreeze All Plots"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1AnalyzerToggleFreezeAll,
                        object: nil
                    )
            }

            Button(
                "Clear Analyzer Displays"
            ) {
                NotificationCenter.default
                    .post(
                        name:
                            .fv1AnalyzerClearAll,
                        object: nil
                    )
            }

            Divider()

            Menu(
                "FFT Size (next session)"
            ) {
                fftChoice(1024)
                fftChoice(2048)
                fftChoice(4096)
                fftChoice(8192)
            }
        }

        CommandGroup(after: .toolbar) {
            Menu("Theme") {
                ForEach(
                    FV1ThemeCatalog.themes,
                    id: \.self
                ) { name in
                    Button {
                        themeName = name
                    } label: {
                        if themeName == name {
                            Label(
                                name,
                                systemImage:
                                    "checkmark"
                            )
                        } else {
                            Text(name)
                        }
                    }
                }
            }

            Menu("Accent Color") {
                ForEach(
                    FV1ThemeCatalog.accents,
                    id: \.self
                ) { name in
                    Button {
                        accentName = name
                    } label: {
                        if accentName == name {
                            Label(
                                name,
                                systemImage:
                                    "checkmark"
                            )
                        } else {
                            Text(name)
                        }
                    }
                }
            }

            Menu("Application Icon") {
                ForEach(
                    FV1ThemeCatalog.icons,
                    id: \.self
                ) { name in
                    Button {
                        applicationIconName =
                            name

                        FV1MacApplicationIconManager
                            .apply(name: name)
                    } label: {
                        if applicationIconName
                            == name {
                            Label(
                                name,
                                systemImage:
                                    "checkmark"
                            )
                        } else {
                            Text(name)
                        }
                    }
                }
            }
        }

        CommandGroup(
            replacing: .help
        ) {
            Button(
                "Show Startup Splash"
            ) {
                FV1MacStartupCoordinator
                    .shared
                    .showStartupSplashAgain()
            }

            Divider()

            Button(
                "About FV-1 Lab…"
            ) {
                MacAboutPresenter.show()
            }
        }
        #endif

        CommandGroup(
            after: .textEditing
        ) {
            Button(
                "Compile & Load"
            ) {
                compileAction?()
            }
            .keyboardShortcut(
                "b",
                modifiers: [
                    .command,
                    .shift
                ]
            )
            .disabled(
                compileAction == nil
            )
        }
    }

    #if os(macOS)
    @ViewBuilder
    private func fftChoice(
        _ size: Int
    ) -> some View {
        Button {
            analyzerFFTSize = size

            NotificationCenter.default
                .post(
                    name:
                        .fv1SetAnalyzerFFTSize,
                    object: size
                )
        } label: {
            if analyzerFFTSize == size {
                Label(
                    "\(size)",
                    systemImage:
                        "checkmark"
                )
            } else {
                Text("\(size)")
            }
        }
    }

    #endif
}

#if os(macOS)
struct FV1MacRootView: View {
    var body: some View {
        WorkspaceView()
            .fv1Theme()
            .background(
                FV1MacMainWindowCapture()
                    .frame(
                        width: 0,
                        height: 0
                    )
            )
    }
}
#else
struct FV1StartupRootView: View {
    @State private var showingSplash =
        true
    @State private var workspaceAppeared =
        false
    @State private var minimumDisplayElapsed =
        false
    @State private var completingSplash =
        false

    @State private var splashProgress =
        8.0
    @State private var splashStatus =
        "Initializing Spin FV-1 Emulator…"

    var body: some View {
        ZStack {
            WorkspaceView()
                .opacity(
                    showingSplash
                        ? 0
                        : 1
                )
                .onAppear {
                    workspaceAppeared =
                        true
                    splashProgress =
                        max(
                            splashProgress,
                            88
                        )
                    splashStatus =
                        "FV-1 Lab workspace initialized…"
                    completeSplashIfReady()
                }

            if showingSplash {
                FV1StartupSplashView(
                    progress:
                        splashProgress,
                    status:
                        splashStatus,
                    version:
                        FV1Engine
                            .versionString
                )
                .transition(.opacity)
                .zIndex(10)
            }
        }
        .task {
            guard showingSplash else {
                return
            }

            splashProgress = 8
            splashStatus =
                "Initializing Spin FV-1 Emulator…"

            await Task.yield()

            _ = FV1Engine
                .versionString
            _ = FV1Engine
                .abiVersion

            splashProgress = 26
            splashStatus =
                "Loading FV-1 SDK and application resources…"

            await Task.yield()

            splashProgress = 42
            splashStatus =
                "Preparing native Apple testbench…"

            try? await Task.sleep(
                nanoseconds:
                    1_750_000_000
            )

            minimumDisplayElapsed =
                true
            completeSplashIfReady()
        }
    }

    private func completeSplashIfReady() {
        guard showingSplash,
              workspaceAppeared,
              minimumDisplayElapsed,
              !completingSplash else {
            return
        }

        completingSplash = true

        Task { @MainActor in
            splashProgress = 98
            splashStatus =
                "FV-1 Lab ready — preparing workspace…"

            try? await Task.sleep(
                nanoseconds:
                    90_000_000
            )

            withAnimation(
                .easeOut(
                    duration: 0.10
                )
            ) {
                splashProgress = 100
            }

            splashStatus = "Ready"

            try? await Task.sleep(
                nanoseconds:
                    180_000_000
            )

            withAnimation(
                .easeOut(
                    duration: 0.28
                )
            ) {
                showingSplash = false
            }
        }
    }
}
#endif

@main
struct FV1LabApp: App {
    #if os(macOS)
    @NSApplicationDelegateAdaptor(
        FV1MacAppDelegate.self
    )
    private var appDelegate
    #endif

    var body: some Scene {
        WindowGroup("FV-1 Lab") {
            #if os(macOS)
            FV1MacRootView()
            #else
            FV1StartupRootView()
            #endif
        }
        .commands {
            FV1Commands()
        }
    }
}
