// ----------------------------------------------------------------------------
//
//  Unit test for arbitrary host packet sizes over Aeolus's 64-frame quantum.
//
// ----------------------------------------------------------------------------

#include "audio_quantum_fifo.h"
#include <cstdio>

struct TestRenderer
{
    unsigned int next;
    unsigned int calls;
};

static void render_test(void *context, float **outputs, int frames)
{
    TestRenderer *state = static_cast<TestRenderer *>(context);
    for (int i = 0; i < frames; ++i)
    {
        const float sample = (float)state->next++;
        outputs[0][i] = sample;
        outputs[1][i] = sample + 1000.0f;
    }
    ++state->calls;
}

int main(void)
{
    AudioQuantumFifo fifo(2);
    TestRenderer state = { 0, 0 };
    const unsigned int packets[] = { 13, 79, 1, 127, 480, 64, 3 };
    unsigned int expected = 0;

    for (unsigned int p = 0; p < sizeof(packets) / sizeof(packets[0]); ++p)
    {
        float left[480];
        float right[480];
        float *outputs[2] = { left, right };

        if (packets[p] > 480 ||
            !fifo.pull(outputs, packets[p], render_test, &state))
            return 1;

        for (unsigned int i = 0; i < packets[p]; ++i)
        {
            if (left[i] != (float)expected ||
                right[i] != (float)expected + 1000.0f)
            {
                std::fprintf(stderr, "sample discontinuity at %u\n", expected);
                return 2;
            }
            ++expected;
        }
    }

    if (state.calls * AudioQuantumFifo::QUANTUM < expected ||
        (state.calls - 1) * AudioQuantumFifo::QUANTUM >= expected)
        return 3;

    return 0;
}
