#ifdef _WIN32
#include "wasapi_audio_output.h"
#include "../source/audio.h"

AeolusWasapiOutput::AeolusWasapiOutput() : _audio(0), _opened(false)
{
}

AeolusWasapiOutput::~AeolusWasapiOutput()
{
    close();
}

bool AeolusWasapiOutput::open(Audio *audio, bool bform)
{
    if (_opened || !audio) return false;
    if (!_host.open(render, this)) return false;

    if (!_audio || !audio->init_host(_host.sample_rate(),
                                     _host.buffer_frames(),
                                     (int)_host.channels(), bform))
    {
        _host.close();
        return false;
    }

    _audio = audio;
    _opened = true;
    return true;
}

bool AeolusWasapiOutput::render(void *context, float **planes,
                                unsigned int frames)
{
    AeolusWasapiOutput *self = static_cast<AeolusWasapiOutput *>(context);
    return self && self->_audio &&
           self->_audio->render_host_packet(planes, frames);
}

bool AeolusWasapiOutput::run()
{
    return _opened && _host.run();
}

void AeolusWasapiOutput::stop()
{
    _host.stop();
}

void AeolusWasapiOutput::close()
{
    _host.close();
    _audio = 0;
    _opened = false;
}
#endif
