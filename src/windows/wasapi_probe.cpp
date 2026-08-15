#include "wasapi_probe.hpp"

#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <cstdint>
#include <sstream>

namespace fv1::windows_frontend {
namespace {

using Microsoft::WRL::ComPtr;

WasapiEndpointInfo endpoint_info(IMMDeviceEnumerator* enumerator, EDataFlow flow) {
    WasapiEndpointInfo info{};
    ComPtr<IMMDevice> endpoint;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(flow, eMultimedia, &endpoint))) return info;

    ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &properties))) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))) {
            if (value.vt == VT_LPWSTR && value.pwszVal) info.name = value.pwszVal;
        }
        PropVariantClear(&value);
    }

    ComPtr<IAudioClient> client;
    if (FAILED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(client.GetAddressOf())))) {
        return info;
    }

    WAVEFORMATEX* format = nullptr;
    if (FAILED(client->GetMixFormat(&format)) || !format) return info;
    info.sample_rate = format->nSamplesPerSec;
    info.channels = format->nChannels;
    info.bits_per_sample = format->wBitsPerSample;
    info.available = true;
    CoTaskMemFree(format);
    return info;
}

} // namespace

WasapiProbeResult probe_default_wasapi_endpoints() {
    WasapiProbeResult result{};
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool owns_com = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        result.diagnostic = L"COM initialization failed; WASAPI endpoint probe unavailable.";
        return result;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    const HRESULT created = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                             IID_PPV_ARGS(&enumerator));
    if (FAILED(created)) {
        result.diagnostic = L"Unable to create the Windows MMDevice enumerator.";
        if (owns_com) CoUninitialize();
        return result;
    }

    result.capture = endpoint_info(enumerator.Get(), eCapture);
    result.render = endpoint_info(enumerator.Get(), eRender);
    if (!result.capture.available && !result.render.available) {
        result.diagnostic = L"No default multimedia capture/render endpoint is currently available.";
    }

    if (owns_com) CoUninitialize();
    return result;
}

} // namespace fv1::windows_frontend
