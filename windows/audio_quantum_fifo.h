// ----------------------------------------------------------------------------
//
//  Aeolus Windows audio host support.
//  Decouples the engine's fixed 64-frame render quantum from device packet sizes.
//
//  Copyright (C) 2026 OpenAI
//  SPDX-License-Identifier: GPL-3.0-or-later
//
// ----------------------------------------------------------------------------

#ifndef AEOLUS_AUDIO_QUANTUM_FIFO_H
#define AEOLUS_AUDIO_QUANTUM_FIFO_H

#include <cstring>

class AudioQuantumFifo
{
public:
    enum { QUANTUM = 64, MAX_CHANNELS = 4 };
    typedef void (*RenderFunction)(void *context, float **outputs, int frames);

    explicit AudioQuantumFifo(int channels) :
        _channels(channels), _read_pos(QUANTUM)
    {
    }

    bool valid(void) const
    {
        return _channels == 2 || _channels == 4;
    }

    // Pull an arbitrary number of planar float frames. RenderFunction is called
    // only for complete engine quanta; the caller owns the destination buffers.
    // No allocation, locking, or device API calls occur in this method.
    bool pull(float **outputs, unsigned int frames,
              RenderFunction render, void *context)
    {
        unsigned int done = 0;
        int count;

        if (!valid() || !outputs || !render) return false;
        for (int c = 0; c < _channels; ++c)
            if (!outputs[c]) return false;

        while (done < frames)
        {
            if (_read_pos == QUANTUM)
            {
                float *planes[MAX_CHANNELS];
                for (int c = 0; c < _channels; ++c)
                    planes[c] = _quantum[c];
                render(context, planes, QUANTUM);
                _read_pos = 0;
            }

            count = (int)(frames - done);
            if (count > QUANTUM - _read_pos)
                count = QUANTUM - _read_pos;

            for (int c = 0; c < _channels; ++c)
                std::memcpy(outputs[c] + done, _quantum[c] + _read_pos,
                            count * sizeof(float));

            _read_pos += count;
            done += count;
        }
        return true;
    }

private:
    int   _channels;
    int   _read_pos;
    float _quantum[MAX_CHANNELS][QUANTUM];
};

#endif
