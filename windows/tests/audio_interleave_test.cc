#include <assert.h>
#include <math.h>
#include "../audio_interleave.h"

int main ()
{
    float left [3] = { 0.1f, 0.2f, 0.3f };
    float right [3] = { -0.1f, -0.2f, -0.3f };
    const float *stereo [2] = { left, right };
    float out2 [6] = { 0 };
    assert (audio_planar_to_interleaved_float (stereo, 2, 3, out2));
    for (int i = 0; i < 3; ++i)
    {
        assert (fabsf (out2 [2 * i] - left [i]) < 1e-7f);
        assert (fabsf (out2 [2 * i + 1] - right [i]) < 1e-7f);
    }

    float a [2] = { 1.0f, 2.0f };
    float b [2] = { 3.0f, 4.0f };
    float c [2] = { 5.0f, 6.0f };
    float d [2] = { 7.0f, 8.0f };
    const float *quad [4] = { a, b, c, d };
    float out4 [8] = { 0 };
    assert (audio_planar_to_interleaved_float (quad, 4, 2, out4));
    const float expected [8] = { 1, 3, 5, 7, 2, 4, 6, 8 };
    for (int i = 0; i < 8; ++i) assert (out4 [i] == expected [i]);

    assert (!audio_planar_to_interleaved_float (0, 2, 3, out2));
    assert (!audio_planar_to_interleaved_float (stereo, 2, 3, 0));
    assert (!audio_planar_to_interleaved_float (stereo, 0, 3, out2));
    const float *bad [2] = { left, 0 };
    assert (!audio_planar_to_interleaved_float (bad, 2, 3, out2));
    return 0;
}
