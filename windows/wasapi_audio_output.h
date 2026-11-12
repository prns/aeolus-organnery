// ----------------------------------------------------------------------------
// Connect the shared-mode WASAPI device host to Aeolus' existing Audio engine.
// ----------------------------------------------------------------------------
#ifndef AEOLUS_WASAPI_OUTPUT_H
#define AEOLUS_WASAPI_OUTPUT_H

#ifdef _WIN32
#include "wasapi_audio.h"

class Audio;

class AeolusWasapiOutput
{
public:
    AeolusWasapiOutput();
    ~AeolusWasapiOutput();

    AeolusWasapiOutput(const AeolusWasapiOutput &) = delete;
    AeolusWasapiOutput &operator=(const AeolusWasapiOutput &) = delete;

    // Opens WASAPI and configures Audio. The caller must connect the model
    // message routes and call Audio::start() before entering run().
    // open(), run(), and close() must execute on the same thread.
    bool open(Audio *audio, bool bform);
    bool run();
    void stop();
    void close();

private:
    static bool render(void *context, float **planes, unsigned int frames);

    WasapiAudioHost _host;
    Audio *_audio;
    bool _opened;
};
#endif
#endif
