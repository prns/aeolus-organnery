// ----------------------------------------------------------------------------
// Convert Aeolus planar float output to the interleaved float layout used by
// common WASAPI shared-mode mix formats. No allocation is performed here.
// ----------------------------------------------------------------------------

#ifndef AEOLUS_AUDIO_INTERLEAVE_H
#define AEOLUS_AUDIO_INTERLEAVE_H

inline bool audio_planar_to_interleaved_float (const float *const *planes,
                                                unsigned int channels,
                                                unsigned int frames,
                                                float *destination)
{
    if (!planes || !destination || !channels || !frames) return false;
    for (unsigned int c = 0; c < channels; ++c)
        if (!planes [c]) return false;

    for (unsigned int f = 0; f < frames; ++f)
        for (unsigned int c = 0; c < channels; ++c)
            destination [f * channels + c] = planes [c] [f];

    return true;
}

#endif
