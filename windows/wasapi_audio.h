// ----------------------------------------------------------------------------
// Event-driven WASAPI shared-mode render host.
// The caller owns the Audio/render context; this class owns the Windows device.
// ----------------------------------------------------------------------------
#ifndef AEOLUS_WASAPI_AUDIO_H
#define AEOLUS_WASAPI_AUDIO_H

#ifdef _WIN32
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

class WasapiAudioHost
{
public:
    typedef bool (*RenderCallback)(void *context, float **planes, unsigned int frames);

    WasapiAudioHost();
    ~WasapiAudioHost();

    WasapiAudioHost(const WasapiAudioHost &) = delete;
    WasapiAudioHost &operator=(const WasapiAudioHost &) = delete;

    // Opens the default render endpoint in shared mode. This method and run()
    // must be called on the same thread. Only 32-bit IEEE-float mix formats
    // with 2 or 4 channels are accepted; no conversion is done in the callback.
    bool open(RenderCallback callback, void *context);
    bool run();                 // Blocking render loop; call stop() from another thread.
    void stop();
    void close();

    unsigned int sample_rate() const { return _sample_rate; }
    unsigned int channels() const { return _channels; }
    unsigned int buffer_frames() const { return _buffer_frames; }

private:
    bool release_all();

    IMMDeviceEnumerator *_enumerator;
    IMMDevice *_device;
    IAudioClient *_audio_client;
    IAudioRenderClient *_render_client;
    WAVEFORMATEX *_mix_format;
    HANDLE _event;
    volatile LONG _stopping;
    bool _com_initialized;
    bool _opened;
    unsigned int _sample_rate;
    unsigned int _channels;
    unsigned int _buffer_frames;
    RenderCallback _callback;
    void *_context;
    float *_planes[4];
    float *_interleaved;
};
#endif
#endif
