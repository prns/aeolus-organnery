// ----------------------------------------------------------------------------
//
//  Windows-port link smoke test for the non-Linux Imidi lifecycle stub.
//  This is not a MIDI functionality test and does not start a worker thread.
//
// ----------------------------------------------------------------------------

#include "imidi.h"

int main (void)
{
    Lfq_u32 qnote (256);
    Lfq_u8 qmidi (256);
    uint16_t midimap [16] = { 0 };

    Imidi midi (&qnote, &qmidi, midimap, "Aeolus");
    return midi.name ()[0] == 'I' ? 1 : 0;
}
