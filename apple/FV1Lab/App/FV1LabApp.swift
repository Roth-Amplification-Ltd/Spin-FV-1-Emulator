import SwiftUI

struct FV1CompileActionKey: FocusedValueKey {
    typealias Value = () -> Void
}

extension FocusedValues {
    var fv1CompileAction: (() -> Void)? {
        get { self[FV1CompileActionKey.self] }
        set { self[FV1CompileActionKey.self] = newValue }
    }
}

struct FV1Commands: Commands {
    @FocusedValue(\.fv1CompileAction)
    private var compileAction

    var body: some Commands {
        #if os(macOS)
        CommandGroup(replacing: .appInfo) {
            Button("About FV-1 Lab") {
                MacAboutPresenter.show()
            }
        }
        #endif

        CommandGroup(after: .textEditing) {
            Button("Compile & Load") {
                compileAction?()
            }
            .keyboardShortcut(
                "b",
                modifiers: [.command, .shift]
            )
            .disabled(compileAction == nil)
        }
    }
}

#if os(macOS)
struct FV1MacRootView: View {
    var body: some View {
        WorkspaceView()
            .background(
                FV1MacMainWindowCapture()
                    .frame(width: 0, height: 0)
            )
    }
}
#else
struct FV1StartupRootView: View {
    @State private var showingSplash = true
    @State private var workspaceAppeared = false
    @State private var minimumDisplayElapsed = false
    @State private var completingSplash = false

    @State private var splashProgress = 8.0
    @State private var splashStatus =
        "Initializing Spin FV-1 Emulator…"

    var body: some View {
        ZStack {
            WorkspaceView()
                .opacity(showingSplash ? 0 : 1)
                .onAppear {
                    workspaceAppeared = true
                    splashProgress = max(
                        splashProgress,
                        88
                    )
                    splashStatus =
                        "FV-1 Lab workspace initialized…"
                    completeSplashIfReady()
                }

            if showingSplash {
                FV1StartupSplashView(
                    progress: splashProgress,
                    status: splashStatus,
                    version: FV1Engine.versionString
                )
                .transition(.opacity)
                .zIndex(10)
            }
        }
        .task {
            guard showingSplash else { return }

            splashProgress = 8
            splashStatus =
                "Initializing Spin FV-1 Emulator…"

            await Task.yield()

            _ = FV1Engine.versionString
            _ = FV1Engine.abiVersion

            splashProgress = 26
            splashStatus =
                "Loading FV-1 SDK and application resources…"

            await Task.yield()

            splashProgress = 42
            splashStatus =
                "Preparing native Apple testbench…"

            try? await Task.sleep(
                nanoseconds: 1_750_000_000
            )

            minimumDisplayElapsed = true
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
                nanoseconds: 90_000_000
            )

            withAnimation(.easeOut(duration: 0.10)) {
                splashProgress = 100
            }

            splashStatus = "Ready"

            try? await Task.sleep(
                nanoseconds: 180_000_000
            )

            withAnimation(.easeOut(duration: 0.28)) {
                showingSplash = false
            }
        }
    }
}
#endif

@main
struct FV1LabApp: App {
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
