#if os(macOS)
import AppKit
import Combine
import QuartzCore
import SwiftUI

@MainActor
private final class FV1MacSplashState: ObservableObject {
    @Published var progress = 8.0
    @Published var status = "Initializing Spin FV-1 Emulator…"

    let version = FV1Engine.versionString
}

private struct FV1MacSplashHostView: View {
    @ObservedObject var state: FV1MacSplashState

    var body: some View {
        FV1StartupSplashView(
            progress: state.progress,
            status: state.status,
            version: state.version
        )
        .frame(width: 720, height: 405)
    }
}

@MainActor
final class FV1MacStartupCoordinator {
    static let shared = FV1MacStartupCoordinator()

    private weak var mainWindow: NSWindow?
    private var splashWindow: NSPanel?
    private var splashState: FV1MacSplashState?
    private var didStart = false
    private var didFinish = false

    private init() {}

    func attachMainWindow(_ window: NSWindow) {
        guard !didFinish else { return }

        mainWindow = window

        // Normalize the real dashboard window before hiding it for the
        // independent splash. SwiftUI can otherwise reveal it in a geometry
        // smaller than the three-column workspace actually requires.
        configureMainWindowGeometry(window)

        window.alphaValue = 0
        window.orderOut(nil)

        guard !didStart else { return }
        didStart = true

        presentSplash()
    }

    private func presentSplash() {
        let state = FV1MacSplashState()
        splashState = state

        let panel = NSPanel(
            contentRect: NSRect(
                x: 0,
                y: 0,
                width: 720,
                height: 405
            ),
            styleMask: [.borderless],
            backing: .buffered,
            defer: false
        )

        panel.title = "FV-1 Lab"
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.level = .floating
        panel.hidesOnDeactivate = false
        panel.isReleasedWhenClosed = false
        panel.collectionBehavior = [
            .canJoinAllSpaces,
            .fullScreenAuxiliary
        ]

        panel.contentViewController = NSHostingController(
            rootView: FV1MacSplashHostView(state: state)
        )
        panel.setContentSize(
            NSSize(width: 720, height: 405)
        )
        panel.center()
        panel.alphaValue = 1

        splashWindow = panel

        NSApplication.shared.activate(
            ignoringOtherApps: true
        )
        panel.orderFrontRegardless()

        Task { @MainActor [weak self] in
            await self?.runStartupSequence()
        }
    }

    private func sleep(milliseconds: UInt64) async {
        try? await Task.sleep(
            nanoseconds: milliseconds * 1_000_000
        )
    }

    private func runStartupSequence() async {
        guard let state = splashState else { return }

        state.progress = 8
        state.status =
            "Initializing Spin FV-1 Emulator…"
        await sleep(milliseconds: 180)

        _ = FV1Engine.versionString
        _ = FV1Engine.abiVersion

        withAnimation(.easeOut(duration: 0.16)) {
            state.progress = 26
        }
        state.status =
            "Loading FV-1 SDK and application resources…"
        await sleep(milliseconds: 310)

        withAnimation(.easeOut(duration: 0.18)) {
            state.progress = 48
        }
        state.status =
            "Preparing native Apple testbench…"
        await sleep(milliseconds: 420)

        withAnimation(.easeOut(duration: 0.20)) {
            state.progress = 76
        }
        state.status =
            "Initializing FV-1 Lab workspace…"
        await sleep(milliseconds: 430)

        withAnimation(.easeOut(duration: 0.18)) {
            state.progress = 98
        }
        state.status =
            "FV-1 Lab ready — preparing workspace…"
        await sleep(milliseconds: 230)

        withAnimation(.easeOut(duration: 0.12)) {
            state.progress = 100
        }
        state.status = "Ready"

        await sleep(milliseconds: 180)

        revealMainWindow()
    }

    private func revealMainWindow() {
        guard !didFinish else { return }
        didFinish = true

        guard let panel = splashWindow else {
            showMainWindowNow()
            return
        }

        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0.28
            context.timingFunction = CAMediaTimingFunction(
                name: .easeOut
            )
            panel.animator().alphaValue = 0
        } completionHandler: { [weak self] in
            Task { @MainActor in
                panel.orderOut(nil)
                panel.close()

                self?.splashWindow = nil
                self?.splashState = nil
                self?.showMainWindowNow()
            }
        }
    }

    private func configureMainWindowGeometry(_ window: NSWindow) {
        let preferred = NSSize(width: 1500, height: 860)
        let baseMinimum = NSSize(width: 1360, height: 760)

        guard let visibleFrame =
            window.screen?.visibleFrame ?? NSScreen.main?.visibleFrame else {
            window.contentMinSize = baseMinimum

            let current = window.contentLayoutRect.size
            let target = NSSize(
                width: max(current.width, preferred.width),
                height: max(current.height, preferred.height)
            )

            if abs(current.width - target.width) > 1
                || abs(current.height - target.height) > 1 {
                window.setContentSize(target)
                window.center()
            }
            return
        }

        // Leave a small margin around the application. On smaller displays,
        // fit the initial window to the usable work area instead of placing
        // part of the dashboard off-screen.
        let availableWidth = max(960, visibleFrame.width - 40)
        let availableHeight = max(640, visibleFrame.height - 40)

        window.contentMinSize = NSSize(
            width: min(baseMinimum.width, availableWidth),
            height: min(baseMinimum.height, availableHeight)
        )

        let desired = NSSize(
            width: min(preferred.width, availableWidth),
            height: min(preferred.height, availableHeight)
        )

        let current = window.contentLayoutRect.size

        // Preserve a user/window size that is already larger, but automatically
        // enlarge undersized SwiftUI startup geometry.
        let target = NSSize(
            width: min(availableWidth, max(current.width, desired.width)),
            height: min(availableHeight, max(current.height, desired.height))
        )

        if abs(current.width - target.width) > 1
            || abs(current.height - target.height) > 1 {
            window.setContentSize(target)
            window.center()
        }
    }

    private func showMainWindowNow() {
        guard let window = mainWindow else { return }

        // SwiftUI may perform late layout while the main window is ordered out,
        // so normalize once more immediately before revealing it.
        configureMainWindowGeometry(window)

        window.alphaValue = 1
        window.makeKeyAndOrderFront(nil)
        NSApplication.shared.activate(
            ignoringOtherApps: true
        )
    }
}

struct FV1MacMainWindowCapture: NSViewRepresentable {
    func makeNSView(context: Context) -> NSView {
        FV1MacWindowCaptureView()
    }

    func updateNSView(
        _ nsView: NSView,
        context: Context
    ) {}
}

private final class FV1MacWindowCaptureView: NSView {
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()

        guard let window else { return }

        Task { @MainActor in
            FV1MacStartupCoordinator.shared
                .attachMainWindow(window)
        }
    }
}
#endif
