#include "wasapi_engine.hpp"

#include "realtime_processor.hpp"

#include <windows.h>
#include <propkeydef.h>
#include <propsys.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace fv1::windows_frontend {
namespace {

using Microsoft::WRL::ComPtr;

constexpr DWORD kStreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                               AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                               AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
constexpr DWORD kRecoveryDelayMs = 350u;
constexpr DWORD kEndpointPollMs = 1000u;
constexpr std::size_t kOutputFifoCapacity = 16384u;

struct StereoFrame {
    float left{};
    float right{};
};

class StereoFifo final {
public:
    bool push(StereoFrame frame) noexcept {
        if (size_ == frames_.size()) return false;
        frames_[write_] = frame;
        write_ = (write_ + 1u) % frames_.size();
        ++size_;
        return true;
    }

    bool pop(StereoFrame& frame) noexcept {
        if (size_ == 0u) return false;
        frame = frames_[read_];
        read_ = (read_ + 1u) % frames_.size();
        --size_;
        return true;
    }

    void clear() noexcept { read_ = write_ = size_ = 0u; }

private:
    std::array<StereoFrame, kOutputFifoCapacity> frames_{};
    std::size_t read_{};
    std::size_t write_{};
    std::size_t size_{};
};

struct EventHandle {
    HANDLE value{};
    ~EventHandle() {
        if (value) CloseHandle(value);
    }
};

struct StreamBundle {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> capture_device;
    ComPtr<IMMDevice> render_device;
    ComPtr<IAudioClient> capture_client;
    ComPtr<IAudioClient> render_client;
    ComPtr<IAudioCaptureClient> capture_service;
    ComPtr<IAudioRenderClient> render_service;
    EventHandle capture_event;
    EventHandle render_event;
    UINT32 render_buffer_frames{};
    std::wstring capture_id;
    std::wstring render_id;
    std::wstring capture_name;
    std::wstring render_name;
};

std::wstring hresult_text(HRESULT result) {
    wchar_t* buffer = nullptr;
    const DWORD count = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring message;
    if (count != 0u && buffer) {
        message.assign(buffer, count);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
        LocalFree(buffer);
    } else {
        std::wostringstream stream;
        stream << L"HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
        message = stream.str();
    }
    return message;
}

std::wstring device_name(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    if (!device || FAILED(device->OpenPropertyStore(STGM_READ, &properties))) return {};
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring name;
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
        value.vt == VT_LPWSTR && value.pwszVal) {
        name = value.pwszVal;
    }
    PropVariantClear(&value);
    return name;
}

std::wstring device_id(IMMDevice* device) {
    if (!device) return {};
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || !id) return {};
    std::wstring result(id);
    CoTaskMemFree(id);
    return result;
}

WAVEFORMATEXTENSIBLE fv1_stream_format() {
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 2u;
    format.Format.nSamplesPerSec = 32768u;
    format.Format.wBitsPerSample = 32u;
    format.Format.nBlockAlign = static_cast<WORD>(format.Format.nChannels * sizeof(float));
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    format.Samples.wValidBitsPerSample = 32u;
    format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return format;
}

HRESULT activate_client(IMMDevice* device, IAudioClient** output) {
    if (!device || !output) return E_POINTER;
    *output = nullptr;
    return device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(output));
}

HRESULT open_streams(StreamBundle& bundle) {
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&bundle.enumerator));
    if (FAILED(result)) return result;

    result = bundle.enumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &bundle.capture_device);
    if (FAILED(result)) return result;
    result = bundle.enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &bundle.render_device);
    if (FAILED(result)) return result;

    bundle.capture_id = device_id(bundle.capture_device.Get());
    bundle.render_id = device_id(bundle.render_device.Get());
    bundle.capture_name = device_name(bundle.capture_device.Get());
    bundle.render_name = device_name(bundle.render_device.Get());

    result = activate_client(bundle.capture_device.Get(), &bundle.capture_client);
    if (FAILED(result)) return result;
    result = activate_client(bundle.render_device.Get(), &bundle.render_client);
    if (FAILED(result)) return result;

    const auto format = fv1_stream_format();
    result = bundle.capture_client->Initialize(AUDCLNT_SHAREMODE_SHARED, kStreamFlags, 0, 0,
                                               &format.Format, nullptr);
    if (FAILED(result)) return result;
    result = bundle.render_client->Initialize(AUDCLNT_SHAREMODE_SHARED, kStreamFlags, 0, 0,
                                              &format.Format, nullptr);
    if (FAILED(result)) return result;

    bundle.capture_event.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    bundle.render_event.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!bundle.capture_event.value || !bundle.render_event.value) return HRESULT_FROM_WIN32(GetLastError());

    result = bundle.capture_client->SetEventHandle(bundle.capture_event.value);
    if (FAILED(result)) return result;
    result = bundle.render_client->SetEventHandle(bundle.render_event.value);
    if (FAILED(result)) return result;

    result = bundle.capture_client->GetService(IID_PPV_ARGS(&bundle.capture_service));
    if (FAILED(result)) return result;
    result = bundle.render_client->GetService(IID_PPV_ARGS(&bundle.render_service));
    if (FAILED(result)) return result;
    return bundle.render_client->GetBufferSize(&bundle.render_buffer_frames);
}

bool default_endpoint_changed(StreamBundle& bundle) {
    ComPtr<IMMDevice> capture;
    ComPtr<IMMDevice> render;
    if (FAILED(bundle.enumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &capture))) return true;
    if (FAILED(bundle.enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &render))) return true;
    return device_id(capture.Get()) != bundle.capture_id || device_id(render.Get()) != bundle.render_id;
}

} // namespace

WasapiAudioEngine::WasapiAudioEngine() {
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    for (auto& pot : pots_) pot.store(0.5F, std::memory_order_relaxed);
    for (auto& sample : scope_) sample.store(0.0F, std::memory_order_relaxed);
}

WasapiAudioEngine::~WasapiAudioEngine() {
    stop();
    if (stop_event_) {
        CloseHandle(static_cast<HANDLE>(stop_event_));
        stop_event_ = nullptr;
    }
}

bool WasapiAudioEngine::start(
    const std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES>& program,
    const std::array<float, 3>& pots) {
    stop();
    if (!stop_event_) {
        set_error(L"Unable to create WASAPI stop event.");
        return false;
    }
    program_ = program;
    set_pots(pots);
    capture_frames_.store(0u, std::memory_order_relaxed);
    render_frames_.store(0u, std::memory_order_relaxed);
    output_underruns_.store(0u, std::memory_order_relaxed);
    output_overruns_.store(0u, std::memory_order_relaxed);
    recoveries_.store(0u, std::memory_order_relaxed);
    scope_write_index_.store(0u, std::memory_order_relaxed);
    stop_requested_.store(false, std::memory_order_release);
    ResetEvent(static_cast<HANDLE>(stop_event_));
    {
        std::lock_guard lock(info_mutex_);
        last_error_.clear();
    }
    worker_ = std::thread(&WasapiAudioEngine::worker_main, this);
    return true;
}

void WasapiAudioEngine::stop() {
    stop_requested_.store(true, std::memory_order_release);
    if (stop_event_) SetEvent(static_cast<HANDLE>(stop_event_));
    if (worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);
    recovering_.store(false, std::memory_order_release);
}

void WasapiAudioEngine::set_pots(const std::array<float, 3>& pots) noexcept {
    for (std::size_t i = 0; i < pots_.size(); ++i) {
        pots_[i].store(std::clamp(pots[i], 0.0F, 1.0F), std::memory_order_relaxed);
    }
    pot_revision_.fetch_add(1u, std::memory_order_release);
}

WasapiStreamStatus WasapiAudioEngine::status() const {
    WasapiStreamStatus result{};
    result.running = running_.load(std::memory_order_acquire);
    result.recovering = recovering_.load(std::memory_order_acquire);
    result.capture_frames = capture_frames_.load(std::memory_order_relaxed);
    result.render_frames = render_frames_.load(std::memory_order_relaxed);
    result.output_underruns = output_underruns_.load(std::memory_order_relaxed);
    result.output_overruns = output_overruns_.load(std::memory_order_relaxed);
    result.recoveries = recoveries_.load(std::memory_order_relaxed);
    std::lock_guard lock(info_mutex_);
    result.capture_name = capture_name_;
    result.render_name = render_name_;
    result.last_error = last_error_;
    return result;
}

void WasapiAudioEngine::copy_scope(std::vector<float>& output, std::size_t maximum_samples) const {
    const std::size_t count = std::min(maximum_samples, kScopeCapacity);
    output.resize(count);
    const auto end = scope_write_index_.load(std::memory_order_acquire);
    const auto start = end > count ? end - count : 0u;
    const std::size_t leading_zeros = count - static_cast<std::size_t>(end - start);
    std::fill_n(output.begin(), leading_zeros, 0.0F);
    for (std::uint64_t index = start; index < end; ++index) {
        const std::size_t destination = leading_zeros + static_cast<std::size_t>(index - start);
        output[destination] = scope_[index % kScopeCapacity].load(std::memory_order_relaxed);
    }
}

void WasapiAudioEngine::set_error(std::wstring message) {
    std::lock_guard lock(info_mutex_);
    last_error_ = std::move(message);
}

void WasapiAudioEngine::set_endpoint_names(std::wstring capture, std::wstring render) {
    std::lock_guard lock(info_mutex_);
    capture_name_ = std::move(capture);
    render_name_ = std::move(render);
}

void WasapiAudioEngine::worker_main() {
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool owns_com = SUCCEEDED(com_result);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
        set_error(L"WASAPI COM initialization failed: " + hresult_text(com_result));
        return;
    }

    RealtimeProcessor processor;
    if (!processor.ready()) {
        set_error(L"Unable to create the realtime FV1SDK engine.");
        if (owns_com) CoUninitialize();
        return;
    }
    if (processor.load_program(program_) != FV1_SDK_OK) {
        set_error(L"Unable to load the current FV-1 program into the realtime engine.");
        if (owns_com) CoUninitialize();
        return;
    }

    StereoFifo output_fifo;
    DWORD mmcss_task_index = 0u;
    HANDLE mmcss_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcss_task_index);

    while (!stop_requested_.load(std::memory_order_acquire)) {
        recovering_.store(true, std::memory_order_release);
        running_.store(false, std::memory_order_release);
        StreamBundle stream;
        const HRESULT opened = open_streams(stream);
        if (FAILED(opened)) {
            set_error(L"Unable to open default WASAPI capture/render endpoints at the FV-1 32.768 kHz stream rate: " +
                      hresult_text(opened));
            recoveries_.fetch_add(1u, std::memory_order_relaxed);
            if (WaitForSingleObject(static_cast<HANDLE>(stop_event_), kRecoveryDelayMs) == WAIT_OBJECT_0) break;
            continue;
        }

        set_endpoint_names(stream.capture_name, stream.render_name);
        set_error(L"");
        output_fifo.clear();
        const std::array<float, 3> initial_pots{
            pots_[0].load(std::memory_order_relaxed),
            pots_[1].load(std::memory_order_relaxed),
            pots_[2].load(std::memory_order_relaxed)};
        (void)processor.set_pots(initial_pots);
        std::uint64_t applied_pot_revision = pot_revision_.load(std::memory_order_acquire);

        BYTE* initial_buffer = nullptr;
        if (stream.render_buffer_frames != 0u &&
            SUCCEEDED(stream.render_service->GetBuffer(stream.render_buffer_frames, &initial_buffer))) {
            (void)stream.render_service->ReleaseBuffer(stream.render_buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT);
        }

        HRESULT result = stream.render_client->Start();
        if (SUCCEEDED(result)) result = stream.capture_client->Start();
        if (FAILED(result)) {
            set_error(L"WASAPI stream start failed: " + hresult_text(result));
            recoveries_.fetch_add(1u, std::memory_order_relaxed);
            if (WaitForSingleObject(static_cast<HANDLE>(stop_event_), kRecoveryDelayMs) == WAIT_OBJECT_0) break;
            continue;
        }

        recovering_.store(false, std::memory_order_release);
        running_.store(true, std::memory_order_release);
        bool reopen = false;

        HANDLE waits[3] = {
            static_cast<HANDLE>(stop_event_), stream.capture_event.value, stream.render_event.value};

        while (!stop_requested_.load(std::memory_order_acquire) && !reopen) {
            const DWORD wait = WaitForMultipleObjects(3u, waits, FALSE, kEndpointPollMs);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_TIMEOUT) {
                if (default_endpoint_changed(stream)) {
                    set_error(L"Default Windows audio endpoint changed; reopening realtime stream.");
                    reopen = true;
                }
                continue;
            }
            if (wait == WAIT_FAILED) {
                set_error(L"WASAPI event wait failed.");
                reopen = true;
                continue;
            }

            const auto revision = pot_revision_.load(std::memory_order_acquire);
            if (revision != applied_pot_revision) {
                const std::array<float, 3> current_pots{
                    pots_[0].load(std::memory_order_relaxed),
                    pots_[1].load(std::memory_order_relaxed),
                    pots_[2].load(std::memory_order_relaxed)};
                (void)processor.set_pots(current_pots);
                applied_pot_revision = revision;
            }

            if (wait == WAIT_OBJECT_0 + 1u) {
                UINT32 packet_frames = 0u;
                result = stream.capture_service->GetNextPacketSize(&packet_frames);
                while (SUCCEEDED(result) && packet_frames != 0u) {
                    BYTE* bytes = nullptr;
                    UINT32 frames = 0u;
                    DWORD flags = 0u;
                    result = stream.capture_service->GetBuffer(&bytes, &frames, &flags, nullptr, nullptr);
                    if (FAILED(result)) break;
                    const float* input = reinterpret_cast<const float*>(bytes);
                    for (UINT32 i = 0u; i < frames; ++i) {
                        float input_left = 0.0F;
                        float input_right = 0.0F;
                        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0u && input) {
                            input_left = input[i * 2u];
                            input_right = input[(i * 2u) + 1u];
                        }
                        float output_left = 0.0F;
                        float output_right = 0.0F;
                        const auto processed = processor.process_sample(
                            input_left, input_right, output_left, output_right);
                        if (processed != FV1_SDK_OK) {
                            output_left = 0.0F;
                            output_right = 0.0F;
                        }
                        if (!output_fifo.push({output_left, output_right})) {
                            StereoFrame discarded{};
                            (void)output_fifo.pop(discarded);
                            (void)output_fifo.push({output_left, output_right});
                            output_overruns_.fetch_add(1u, std::memory_order_relaxed);
                        }
                        const auto scope_index = scope_write_index_.fetch_add(1u, std::memory_order_acq_rel);
                        scope_[scope_index % kScopeCapacity].store(
                            (output_left + output_right) * 0.5F, std::memory_order_relaxed);
                    }
                    capture_frames_.fetch_add(frames, std::memory_order_relaxed);
                    result = stream.capture_service->ReleaseBuffer(frames);
                    if (FAILED(result)) break;
                    result = stream.capture_service->GetNextPacketSize(&packet_frames);
                }
                if (FAILED(result)) {
                    set_error(L"WASAPI capture failed: " + hresult_text(result));
                    reopen = true;
                }
            }

            if (!reopen && wait == WAIT_OBJECT_0 + 2u) {
                UINT32 padding = 0u;
                result = stream.render_client->GetCurrentPadding(&padding);
                if (SUCCEEDED(result)) {
                    const UINT32 available = stream.render_buffer_frames > padding
                        ? stream.render_buffer_frames - padding : 0u;
                    if (available != 0u) {
                        BYTE* bytes = nullptr;
                        result = stream.render_service->GetBuffer(available, &bytes);
                        if (SUCCEEDED(result)) {
                            float* output = reinterpret_cast<float*>(bytes);
                            for (UINT32 i = 0u; i < available; ++i) {
                                StereoFrame frame{};
                                if (!output_fifo.pop(frame)) {
                                    output_underruns_.fetch_add(1u, std::memory_order_relaxed);
                                }
                                output[i * 2u] = frame.left;
                                output[(i * 2u) + 1u] = frame.right;
                            }
                            result = stream.render_service->ReleaseBuffer(available, 0u);
                            if (SUCCEEDED(result)) {
                                render_frames_.fetch_add(available, std::memory_order_relaxed);
                            }
                        }
                    }
                }
                if (FAILED(result)) {
                    set_error(L"WASAPI render failed: " + hresult_text(result));
                    reopen = true;
                }
            }

        }

        running_.store(false, std::memory_order_release);
        (void)stream.capture_client->Stop();
        (void)stream.render_client->Stop();
        if (!stop_requested_.load(std::memory_order_acquire)) {
            recovering_.store(true, std::memory_order_release);
            recoveries_.fetch_add(1u, std::memory_order_relaxed);
            if (WaitForSingleObject(static_cast<HANDLE>(stop_event_), kRecoveryDelayMs) == WAIT_OBJECT_0) break;
        }
    }

    running_.store(false, std::memory_order_release);
    recovering_.store(false, std::memory_order_release);
    if (mmcss_handle) AvRevertMmThreadCharacteristics(mmcss_handle);
    if (owns_com) CoUninitialize();
}

} // namespace fv1::windows_frontend
