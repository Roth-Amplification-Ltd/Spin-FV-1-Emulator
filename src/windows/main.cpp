#include "fv1_session.hpp"
#include "resource.h"
#include "wasapi_probe.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef FV1_PRODUCT_VERSION_STRING
#define FV1_PRODUCT_VERSION_STRING "development"
#endif

namespace {

using fv1::windows_frontend::Session;

constexpr wchar_t kWindowClass[] = L"RothFV1LabNativeWindow";
constexpr wchar_t kWindowTitle[] = L"FV-1 Lab — Native Windows Preview";
constexpr UINT_PTR kProbeTimer = 1u;
constexpr UINT kProbeIntervalMs = 80u;
constexpr int kMargin = 10;
constexpr int kGap = 8;

struct AppState {
    Session session;
    HWND source{};
    HWND compile_button{};
    HWND reset_button{};
    std::array<HWND, 3> pots{};
    HWND scope{};
    HWND resources{};
    HWND snapshot{};
    HWND console{};
    HWND status{};
    std::vector<float> scope_samples;
};

std::wstring utf8_to_wide(std::string_view input) {
    if (input.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                          static_cast<int>(input.size()), nullptr, 0);
    if (count <= 0) return L"<invalid UTF-8>";
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
                        out.data(), count);
    return out;
}

std::string wide_to_utf8(std::wstring_view input) {
    if (input.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(),
                                          static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string out(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
                        out.data(), count, nullptr, nullptr);
    return out;
}

void set_text(HWND control, std::wstring_view text) {
    SetWindowTextW(control, std::wstring(text).c_str());
}

std::wstring get_text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(std::max(length, 0)) + 1u, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(std::max(copied, 0)));
    return text;
}

void append_console(AppState& state, std::wstring_view line) {
    if (!state.console) return;
    const int length = GetWindowTextLengthW(state.console);
    SendMessageW(state.console, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    std::wstring payload(line);
    payload += L"\r\n";
    SendMessageW(state.console, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(payload.c_str()));
}

std::wstring choose_file(HWND owner, const wchar_t* filter) {
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return {};
    return path.data();
}

bool read_text_file(const std::wstring& path, std::string& output) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    output = stream.str();
    if (output.size() >= 3u && static_cast<unsigned char>(output[0]) == 0xEFu &&
        static_cast<unsigned char>(output[1]) == 0xBBu && static_cast<unsigned char>(output[2]) == 0xBFu) {
        output.erase(0, 3);
    }
    return true;
}

bool read_binary_file(const std::wstring& path, std::vector<std::uint8_t>& output) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

void compile_source(AppState& state) {
    const std::string source = wide_to_utf8(get_text(state.source));
    const auto result = state.session.compile_and_load(source);
    if (result.result == FV1_SDK_OK) {
        std::wostringstream message;
        message << L"SpinASM loaded: " << result.report.instruction_count << L" instructions";
        if (result.report.highest_delay_address != 0u) {
            message << L", highest static delay address " << result.report.highest_delay_address;
        }
        append_console(state, message.str());
        set_text(state.status, L"PROGRAM LOADED • deterministic native probe active");
    } else {
        std::wstring message = L"Compile/load failed: ";
        message += utf8_to_wide(fv1_sdk_result_string(result.result));
        if (!result.diagnostic.empty()) {
            message += L" — ";
            message += utf8_to_wide(result.diagnostic);
        }
        append_console(state, message);
        set_text(state.status, L"PROGRAM ERROR");
    }
}

void update_telemetry(AppState& state) {
    if (!state.session.program_loaded()) return;
    if (state.session.run_probe(256u, state.scope_samples) == FV1_SDK_OK) {
        InvalidateRect(state.scope, nullptr, FALSE);
    }

    fv1_sdk_snapshot_v1 snapshot{};
    if (state.session.snapshot(snapshot) == FV1_SDK_OK) {
        std::wostringstream text;
        text << L"VIRTUAL CHIP\r\n"
             << L"sample       " << snapshot.sample_counter << L"\r\n"
             << L"PC           " << snapshot.program_counter << L"\r\n"
             << L"instruction  " << snapshot.instruction_counter << L"\r\n"
             << L"ACC          " << snapshot.acc << L"\r\n"
             << L"PACC         " << snapshot.pacc << L"\r\n"
             << L"LR           " << snapshot.lr << L"\r\n"
             << L"ADDR_PTR     " << snapshot.delay_pointer << L"\r\n"
             << L"DACL         " << snapshot.regs[FV1_SDK_REG_DACL] << L"\r\n"
             << L"DACR         " << snapshot.regs[FV1_SDK_REG_DACR] << L"\r\n"
             << L"POT0         " << snapshot.regs[FV1_SDK_REG_POT0] << L"\r\n"
             << L"POT1         " << snapshot.regs[FV1_SDK_REG_POT1] << L"\r\n"
             << L"POT2         " << snapshot.regs[FV1_SDK_REG_POT2];
        set_text(state.snapshot, text.str());
    }

    fv1_sdk_resource_report_v1 report{};
    if (state.session.resources(report) == FV1_SDK_OK) {
        std::wostringstream text;
        text << L"RESOURCE METER\r\n"
             << L"instructions       " << report.used_instructions << L" / 128\r\n"
             << L"worst-case path    " << report.worst_case_path << L"\r\n"
             << L"delay reads        " << report.static_delay_reads + report.dynamic_delay_reads << L"\r\n"
             << L"delay writes       " << report.static_delay_writes << L"\r\n"
             << L"highest delay addr " << report.highest_static_delay_address << L"\r\n"
             << L"general registers  " << report.general_registers_used << L"\r\n"
             << L"POTs used          " << report.pots_used << L"\r\n"
             << L"sine LFOs          " << report.sine_lfos_used << L"\r\n"
             << L"ramp LFOs          " << report.ramp_lfos_used << L"\r\n"
             << L"SKP instructions   " << report.skip_instructions;
        set_text(state.resources, text.str());
    }
}

void probe_wasapi(AppState& state) {
    const auto probe = fv1::windows_frontend::probe_default_wasapi_endpoints();
    append_console(state, L"WASAPI default endpoint probe:");
    auto print_endpoint = [&](std::wstring_view label, const auto& endpoint) {
        std::wostringstream text;
        text << L"  " << label << L": ";
        if (!endpoint.available) {
            text << L"not available";
        } else {
            text << (endpoint.name.empty() ? L"<unnamed>" : endpoint.name)
                 << L" — " << endpoint.sample_rate << L" Hz, "
                 << endpoint.channels << L" ch, " << endpoint.bits_per_sample << L" bit";
        }
        append_console(state, text.str());
    };
    print_endpoint(L"capture", probe.capture);
    print_endpoint(L"render", probe.render);
    if (!probe.diagnostic.empty()) append_console(state, L"  " + probe.diagnostic);
    append_console(state, L"  Phase 7A probes native endpoints; realtime duplex streaming remains the next Windows-audio increment.");
}

HMENU build_menu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_FILE_OPEN_SPINASM, L"Open SpinASM…");
    AppendMenuW(file, MF_STRING, IDM_FILE_OPEN_PROGRAM, L"Open 512-byte program…");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_FILE_EXIT, L"Exit");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");

    HMENU audio = CreatePopupMenu();
    AppendMenuW(audio, MF_STRING, IDM_AUDIO_PROBE, L"Probe default WASAPI endpoints");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(audio), L"Audio");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_HELP_ABOUT, L"About FV-1 Lab…");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"Help");
    return menu;
}

HWND make_control(HWND parent, const wchar_t* klass, const wchar_t* text, DWORD style,
                  int id, DWORD ex_style = 0) {
    return CreateWindowExW(ex_style, klass, text, WS_CHILD | WS_VISIBLE | style,
                           0, 0, 1, 1, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

void create_controls(HWND window, AppState& state) {
    const DWORD edit_style = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER;
    state.source = make_control(window, L"EDIT", L"", edit_style, IDC_SOURCE, WS_EX_CLIENTEDGE);
    state.compile_button = make_control(window, L"BUTTON", L"Compile && Load", BS_PUSHBUTTON, IDC_COMPILE_LOAD);
    state.reset_button = make_control(window, L"BUTTON", L"Reset chip", BS_PUSHBUTTON, IDC_RESET);

    for (std::size_t i = 0; i < state.pots.size(); ++i) {
        state.pots[i] = make_control(window, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS,
                                     IDC_POT0 + static_cast<int>(i));
        SendMessageW(state.pots[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
        SendMessageW(state.pots[i], TBM_SETPOS, TRUE, 500);
        state.session.set_pot(static_cast<std::uint32_t>(i), 0.5F);
    }

    state.scope = make_control(window, L"STATIC", L"", SS_OWNERDRAW | WS_BORDER, IDC_SCOPE);
    state.resources = make_control(window, L"EDIT", L"RESOURCE METER", ES_MULTILINE | ES_READONLY | WS_BORDER,
                                   IDC_RESOURCES, WS_EX_CLIENTEDGE);
    state.snapshot = make_control(window, L"EDIT", L"VIRTUAL CHIP", ES_MULTILINE | ES_READONLY | WS_BORDER,
                                  IDC_SNAPSHOT, WS_EX_CLIENTEDGE);
    state.console = make_control(window, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                 WS_VSCROLL | WS_BORDER, IDC_CONSOLE, WS_EX_CLIENTEDGE);
    state.status = make_control(window, L"STATIC", L"READY", SS_LEFT | WS_BORDER, IDC_STATUS);

    const wchar_t* default_source =
        L"; Native Windows Phase 7A default program\r\n"
        L"RDAX ADCL, 1.0\r\n"
        L"WRAX DACL, 0\r\n"
        L"RDAX ADCR, 1.0\r\n"
        L"WRAX DACR, 0\r\n";
    set_text(state.source, default_source);
    append_console(state, L"FV-1 Lab native Windows frontend initialized.");
    append_console(state, L"Emulator boundary: public FV1SDK C ABI only.");
    compile_source(state);
    probe_wasapi(state);
}

void layout(HWND window, AppState& state) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int status_h = 24;
    const int body_h = std::max(100, height - status_h - (2 * kMargin));
    const int available_w = std::max(300, width - (2 * kMargin) - (2 * kGap));
    const int left_w = available_w * 28 / 100;
    const int center_w = available_w * 42 / 100;
    const int right_w = available_w - left_w - center_w;
    const int x_left = kMargin;
    const int x_center = x_left + left_w + kGap;
    const int x_right = x_center + center_w + kGap;
    const int y = kMargin;

    const int button_h = 30;
    const int pot_h = 34;
    const int pot_block = (pot_h * 3) + (kGap * 2);
    const int source_h = std::max(120, body_h - button_h - pot_block - (kGap * 4));
    MoveWindow(state.source, x_left, y, left_w, source_h, TRUE);
    const int button_w = (left_w - kGap) / 2;
    MoveWindow(state.compile_button, x_left, y + source_h + kGap, button_w, button_h, TRUE);
    MoveWindow(state.reset_button, x_left + button_w + kGap, y + source_h + kGap, button_w, button_h, TRUE);
    int pot_y = y + source_h + kGap + button_h + kGap;
    for (auto pot : state.pots) {
        MoveWindow(pot, x_left, pot_y, left_w, pot_h, TRUE);
        pot_y += pot_h + kGap;
    }

    const int scope_h = body_h * 58 / 100;
    MoveWindow(state.scope, x_center, y, center_w, scope_h, TRUE);
    MoveWindow(state.resources, x_center, y + scope_h + kGap, center_w,
               body_h - scope_h - kGap, TRUE);

    const int snapshot_h = body_h * 45 / 100;
    MoveWindow(state.snapshot, x_right, y, right_w, snapshot_h, TRUE);
    MoveWindow(state.console, x_right, y + snapshot_h + kGap, right_w,
               body_h - snapshot_h - kGap, TRUE);
    MoveWindow(state.status, kMargin, height - status_h - kMargin, width - (2 * kMargin), status_h, TRUE);
}

void draw_scope(const DRAWITEMSTRUCT& draw, const AppState& state) {
    HDC dc = draw.hDC;
    RECT rect = draw.rcItem;
    HBRUSH background = CreateSolidBrush(RGB(12, 16, 18));
    FillRect(dc, &rect, background);
    DeleteObject(background);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(44, 55, 59));
    HPEN old_pen = static_cast<HPEN>(SelectObject(dc, grid_pen));
    const int mid_y = (rect.top + rect.bottom) / 2;
    MoveToEx(dc, rect.left, mid_y, nullptr);
    LineTo(dc, rect.right, mid_y);
    for (int i = 1; i < 4; ++i) {
        const int x = rect.left + ((rect.right - rect.left) * i / 4);
        MoveToEx(dc, x, rect.top, nullptr);
        LineTo(dc, x, rect.bottom);
    }
    SelectObject(dc, old_pen);
    DeleteObject(grid_pen);

    if (state.scope_samples.size() > 1u) {
        HPEN wave_pen = CreatePen(PS_SOLID, 2, RGB(66, 208, 192));
        old_pen = static_cast<HPEN>(SelectObject(dc, wave_pen));
        const int w = std::max(1L, rect.right - rect.left - 1L);
        const int h = std::max(1L, rect.bottom - rect.top - 1L);
        for (std::size_t i = 0; i < state.scope_samples.size(); ++i) {
            const int x = rect.left + static_cast<int>((i * static_cast<std::size_t>(w)) /
                                                       (state.scope_samples.size() - 1u));
            const float clamped = std::clamp(state.scope_samples[i], -1.0F, 1.0F);
            const int y = mid_y - static_cast<int>(clamped * static_cast<float>(h) * 0.45F);
            if (i == 0u) MoveToEx(dc, x, y, nullptr);
            else LineTo(dc, x, y);
        }
        SelectObject(dc, old_pen);
        DeleteObject(wave_pen);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(190, 205, 208));
    RECT label = rect;
    label.left += 8;
    label.top += 6;
    DrawTextW(dc, L"VIRTUAL OUTPUT SCOPE • 440 Hz deterministic probe", -1, &label,
              DT_LEFT | DT_TOP | DT_SINGLELINE);
}

void on_open_spinasm(HWND window, AppState& state) {
    const std::wstring path = choose_file(window, L"SpinASM source (*.spn)\0*.spn\0All files (*.*)\0*.*\0\0");
    if (path.empty()) return;
    std::string source;
    if (!read_text_file(path, source)) {
        append_console(state, L"Unable to read selected SpinASM source.");
        return;
    }
    set_text(state.source, utf8_to_wide(source));
    compile_source(state);
}

void on_open_program(HWND window, AppState& state) {
    const std::wstring path = choose_file(window, L"FV-1 program (*.bin)\0*.bin\0All files (*.*)\0*.*\0\0");
    if (path.empty()) return;
    std::vector<std::uint8_t> program;
    if (!read_binary_file(path, program)) {
        append_console(state, L"Unable to read selected program image.");
        return;
    }
    const auto result = state.session.load_program(program.data(), program.size());
    if (result == FV1_SDK_OK) append_console(state, L"512-byte FV-1 program image loaded.");
    else append_console(state, L"Program load failed: " + utf8_to_wide(fv1_sdk_result_string(result)));
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            state = static_cast<AppState*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            create_controls(window, *state);
            SetTimer(window, kProbeTimer, kProbeIntervalMs, nullptr);
            return 0;
        }
        case WM_SIZE:
            if (state) layout(window, *state);
            return 0;
        case WM_HSCROLL:
            if (state) {
                HWND control = reinterpret_cast<HWND>(lparam);
                for (std::size_t i = 0; i < state->pots.size(); ++i) {
                    if (control == state->pots[i]) {
                        const LRESULT position = SendMessageW(control, TBM_GETPOS, 0, 0);
                        state->session.set_pot(static_cast<std::uint32_t>(i),
                                               static_cast<float>(position) / 1000.0F);
                        return 0;
                    }
                }
            }
            break;
        case WM_COMMAND:
            if (!state) break;
            switch (LOWORD(wparam)) {
                case IDC_COMPILE_LOAD: compile_source(*state); return 0;
                case IDC_RESET:
                    if (state->session.reset(true) == FV1_SDK_OK) append_console(*state, L"Virtual chip reset; delay RAM cleared.");
                    return 0;
                case IDM_FILE_OPEN_SPINASM: on_open_spinasm(window, *state); return 0;
                case IDM_FILE_OPEN_PROGRAM: on_open_program(window, *state); return 0;
                case IDM_AUDIO_PROBE: probe_wasapi(*state); return 0;
                case IDM_HELP_ABOUT: {
                    std::wstring text = L"FV-1 Lab — Native Windows Preview\n\nVersion ";
                    text += utf8_to_wide(FV1_PRODUCT_VERSION_STRING);
                    text += L"\nFV1SDK ABI 1.0 candidate\n\nCreated & engineered by Adam Vadala-Roth\nRoth Amplification LTD\nMozilla Public License 2.0\n© 2026 Roth Amplification LTD";
                    MessageBoxW(window, text.c_str(), L"About FV-1 Lab", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                case IDM_FILE_EXIT: DestroyWindow(window); return 0;
                default: break;
            }
            break;
        case WM_TIMER:
            if (state && wparam == kProbeTimer) update_telemetry(*state);
            return 0;
        case WM_DRAWITEM:
            if (state) {
                const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
                if (draw && draw->CtlID == IDC_SCOPE) {
                    draw_scope(*draw, *state);
                    return TRUE;
                }
            }
            break;
        case WM_DESTROY:
            KillTimer(window, kProbeTimer);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    // Available on supported Windows 10/11 systems; failure simply leaves
    // legacy DPI behavior rather than preventing the lab from launching.
    if (auto set_dpi = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))) {
        set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_FV1_APP));
    window_class.hIconSm = window_class.hIcon;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class)) return 1;

    AppState state{};
    if (!state.session.ready()) {
        MessageBoxW(nullptr, utf8_to_wide(state.session.last_error()).c_str(),
                    L"FV-1 Lab SDK initialization failed", MB_OK | MB_ICONERROR);
        return 2;
    }

    HWND window = CreateWindowExW(0, kWindowClass, kWindowTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900,
                                  nullptr, build_menu(), instance, &state);
    if (!window) return 3;

    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
