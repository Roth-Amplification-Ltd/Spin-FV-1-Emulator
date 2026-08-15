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

@main
struct FV1LabApp: App {
    var body: some Scene {
        DocumentGroup(newDocument: FV1Document()) { configuration in
            WorkspaceView(document: configuration.$document)
        }
        .commands { FV1Commands() }
    }
}
