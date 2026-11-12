#ifdef _WIN32
#include "wasapi_audio.h"
#include "audio_interleave.h"
#include <ksmedia.h>
#include <new>
#include <string.h>

WasapiAudioHost::WasapiAudioHost() :
    _enumerator(0), _device(0), _audio_client(0), _render_client(0),
    _mix_format(0), _event(0), _stopping(0), _com_initialized(false),
    _opened(false), _sample_rate(0), _channels(0), _buffer_frames(0),
    _callback(0), _context(0), _interleaved(0)
{
    for (unsigned int i = 0; i < 4; ++i) _planes[i] = 0;
}

WasapiAudioHost::~WasapiAudioHost()
{
    close();
}

bool WasapiAudioHost::open(RenderCallback callback, void *context)
{
    HRESULT hr;
    if (_opened || !callback) return false;

    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;
    _com_initialized = true;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void **>(&_enumerator));
    if (FAILED(hr)) return release_all(), false;

    hr = _enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_device);
    if (FAILED(hr)) return release_all(), false;
    hr = _device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0,
                           reinterpret_cast<void **>(&_audio_client));
    if (FAILED(hr)) return release_all(), false;
    hr = _audio_client->GetMixFormat(&_mix_format);
    if (FAILED(hr) || !_mix_format) return release_all(), false;

    bool is_float = (_mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (_mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        _mix_format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
    {
        const WAVEFORMATEXTENSIBLE *ext =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(_mix_format);
        is_float = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
    }

    _channels = _mix_format->nChannels;
    _sample_rate = _mix_format->nSamplesPerSec;
    if (!is_float || _mix_format->wBitsPerSample != 32 ||
        (_channels != 2 && _channels != 4) ||
        _mix_format->nBlockAlign != _channels * sizeof(float) ||
        !_sample_rate)
        return release_all(), false;

    hr = _audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
             AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, _mix_format, 0);
    if (FAILED(hr)) return release_all(), false;

    _event = CreateEventW(0, FALSE, FALSE, 0);
    if (!_event) return release_all(), false;
    hr = _audio_client->SetEventHandle(_event);
    if (FAILED(hr)) return release_all(), false;
    hr = _audio_client->GetBufferSize(&_buffer_frames);
    if (FAILED(hr) || !_buffer_frames) return release_all(), false;
    hr = _audio_client->GetService(__uuidof(IAudioRenderClient),
                                   reinterpret_cast<void **>(&_render_client));
    if (FAILED(hr)) return release_all(), false;

    for (unsigned int i = 0; i < _channels; ++i)
    {
        _planes[i] = new (std::nothrow) float[_buffer_frames];
        if (!_planes[i]) return release_all(), false;
    }
    _interleaved = new (std::nothrow) float[_buffer_frames * _channels];
    if (!_interleaved) return release_all(), false;

    _callback = callback;
    _context = context;
    _stopping = 0;
    _opened = true;
    return true;
}

bool WasapiAudioHost::run()
{
    if (!_opened || !_audio_client || !_render_client) return false;
    HRESULT hr = _audio_client->Start();
    if (FAILED(hr)) return false;

    bool ok = true;
    while (InterlockedCompareExchange(&_stopping, 0, 0) == 0)
    {
        DWORD wait = WaitForSingleObject(_event, 2000);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) { ok = false; break; }

        UINT32 padding = 0;
        hr = _audio_client->GetCurrentPadding(&padding);
        if (FAILED(hr) || padding > _buffer_frames) { ok = false; break; }
        UINT32 frames = _buffer_frames - padding;
        if (!frames) continue;

        BYTE *buffer = 0;
        hr = _render_client->GetBuffer(frames, &buffer);
        if (FAILED(hr) || !buffer) { ok = false; break; }

        bool rendered = _callback(_context, _planes, frames);
        if (rendered)
            rendered = audio_planar_to_interleaved_float(
                const_cast<const float *const *>(_planes), _channels,
                frames, reinterpret_cast<float *>(buffer));

        hr = _render_client->ReleaseBuffer(
            frames, rendered ? 0 : AUDCLNT_BUFFERFLAGS_SILENT);
        if (FAILED(hr)) { ok = false; break; }
    }

    _audio_client->Stop();
    return ok;
}

void WasapiAudioHost::stop()
{
    InterlockedExchange(&_stopping, 1);
    if (_event) SetEvent(_event);
}

bool WasapiAudioHost::release_all()
{
    bool uninit = _com_initialized;
    delete[] _interleaved;
    _interleaved = 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
        delete[] _planes[i];
        _planes[i] = 0;
    }
    if (_render_client) { _render_client->Release(); _render_client = 0; }
    if (_audio_client) { _audio_client->Release(); _audio_client = 0; }
    if (_mix_format) { CoTaskMemFree(_mix_format); _mix_format = 0; }
    if (_device) { _device->Release(); _device = 0; }
    if (_enumerator) { _enumerator->Release(); _enumerator = 0; }
    if (_event) { CloseHandle(_event); _event = 0; }
    _opened = false;
    _callback = 0;
    _context = 0;
    _sample_rate = _channels = _buffer_frames = 0;
    _com_initialized = false;
    if (uninit) CoUninitialize();
    return true;
}

void WasapiAudioHost::close()
{
    if (_opened && _audio_client) _audio_client->Stop();
    release_all();
}
#endif
