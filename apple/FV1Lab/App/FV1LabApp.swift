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
    @FocusedValue(\.fv1CompileAction) private var compileAction

    var body: some Commands {
        #if os(macOS)
        CommandGroup(replacing: .appInfo) {
            Button("About FV-1 Lab") { MacAboutPresenter.show() }
        }
        #endif

        CommandGroup(after: .textEditing) {
            Button("Compile & Load") { compileAction?() }
                .keyboardShortcut("b", modifiers: [.command, .shift])
                .disabled(compileAction == nil)
        }
    }
}

struct FV1StartupRootView: View {
    @State private var showingSplash = true

    var body: some View {
        ZStack {
            WorkspaceView()
                .opacity(showingSplash ? 0 : 1)

            if showingSplash {
                FV1StartupSplashView()
                    .transition(.opacity)
                    .zIndex(10)
            }
        }
        .task {
            guard showingSplash else { return }
            try? await Task.sleep(nanoseconds: 1_350_000_000)
            withAnimation(.easeOut(duration: 0.22)) {
                showingSplash = false
            }
        }
    }
}

struct FV1StartupSplashView: View {
    var body: some View {
        ZStack {
            Color.black

            Image("Splash")
                .resizable()
                .scaledToFit()
                .padding(18)
                .accessibilityLabel("FV-1 Lab")
        }
        .ignoresSafeArea()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

@main
struct FV1LabApp: App {
    var body: some Scene {
        WindowGroup("FV-1 Lab") {
            FV1StartupRootView()
        }
        .commands { FV1Commands() }
    }
}
