// ----------------------------------------------------------------------------
//
//  Copyright (C) 2003-2019 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------


#include "imidi.h"
#include <string.h>

#ifdef __linux__



Imidi::Imidi (Lfq_u32 *qnote, Lfq_u8 *qmidi, uint16_t *midimap, const char *appname) :
    A_thread ("Imidi"),
    _qnote (qnote),
    _qmidi (qmidi),
    _midimap (midimap),
    _appname (appname)
#ifdef __linux__
    , _handle (0)
#endif
{
}


Imidi::~Imidi (void)
{
}


void Imidi::terminate (void)
{
    snd_seq_event_t E;

    if (_handle)
    {
        snd_seq_ev_clear (&E);
        snd_seq_ev_set_direct (&E);
        E.type = SND_SEQ_EVENT_USR0;
        E.source.port = _opport;
        E.dest.client = _client;
        E.dest.port   = _ipport;
        snd_seq_event_output_direct (_handle, &E);
    }
}


void Imidi::thr_main (void)
{
    open_midi ();
    proc_midi ();
    close_midi ();
    send_event (EV_EXIT, 1);
}


void Imidi::open_midi (void)
{
    snd_seq_client_info_t *C;
    M_midi_info *M;

    if (snd_seq_open (&_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0)
    {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }

    snd_seq_client_info_alloca (&C);
    snd_seq_get_client_info (_handle, C);
    _client = snd_seq_client_info_get_client (C);
    snd_seq_client_info_set_name (C, _appname);
    snd_seq_set_client_info (_handle, C);

    if ((_ipport = snd_seq_create_simple_port (_handle, "In",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION)) < 0)
    {
        fprintf(stderr, "Error creating sequencer input port.\n");
        exit(1);
    }

    if ((_opport = snd_seq_create_simple_port (_handle, "Out",
         SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
         SND_SEQ_PORT_TYPE_APPLICATION)) < 0)
    {
        fprintf(stderr, "Error creating sequencer output port.\n");
        exit(1);
    }

    M = new M_midi_info ();
    M->_client = _client;
    M->_ipport = _ipport;
    M->_opport = _opport;
    M->_seq = _handle;
    memcpy (M->_chbits, _midimap, 16 * sizeof (uint16_t));
    send_event (TO_MODEL, M);
}


void Imidi::close_midi (void)
{
    if (_handle) snd_seq_close (_handle);
}


void Imidi::proc_midi (void)
{
    snd_seq_event_t  *E;
    int              c, f, m, n, p, t, v, g;

    // Read and process MIDI commands from the ALSA port.
    // Events related to keyboard state are sent to the
    // audio thread via the qnote queue. All the rest is
    // sent as raw MIDI to the model thread via qmidi.

    while (true)
    {
        snd_seq_event_input(_handle, &E);
        c = E->data.note.channel;
        m = _midimap [c] & 127;        // Keyboard and hold bits
//        d = (_midimap [c] >>  8) & 7;  // Division number if (f & 2)
        f = (_midimap [c] >> 12) & 7;  // Control enabled if (f & 4)

        t = E->type;
        switch (t)
        {
            case SND_SEQ_EVENT_NOTEON:
            case SND_SEQ_EVENT_NOTEOFF:
                n = E->data.note.note;
                v = E->data.note.velocity;
                // get interface group attached to midi channel
                g = 0;
                switch (m & 0x0F) {
                    case 0x01:
                        g = 0;
                    break;
                    case 0x02:
                        g = 1;
                    break;
                    case 0x04:
                        g = 2;
                    break;
                    case 0x08:
                        g = 3;
                    break;
                    case 0x0F:
                        g = 4;
                    break;
                }
                if ((t == SND_SEQ_EVENT_NOTEON) && v)
                {
                    // Note on.
                    if (n < 36)
                    {
                        if ((f & 4) && (n >= 0) && (n < 24))
                        {
                            // Stop enable, sent to model thread
                            if (_qmidi->write_avail () >= 3)
                            {
                            // send group and set on
                            _qmidi->write (0, 0xB0 | c);
                            _qmidi->write (1, MIDICTL_IFELM);
                            _qmidi->write (2, 0x60 | g);
                            _qmidi->write_commit (3);
                            // send stop number
                            _qmidi->write (0, 0xB0 | c);
                            _qmidi->write (1, MIDICTL_IFELM);
                            _qmidi->write (2, n); // stop number based on note
                            _qmidi->write_commit (3);

                            }
                        }

                        if ((f & 4) && (n >= 24) && (n < 34))
                        {
                            // Preset selection, sent to model thread
                            // if on control-enabled channel.
                            if (_qmidi->write_avail () >= 3)
                            {
                                _qmidi->write (0, 0x90);
                                _qmidi->write (1, n);
                                _qmidi->write (2, v);
                                _qmidi->write_commit (3);
                            }
                        }
                    }
                    else if (n <= 96)
                    {
                        if (m)
                        {
                            if (_qnote->write_avail () > 0)
                            {
                                _qnote->write (0, (1 << 24) | ((n - 36) << 8) | m);
                                    _qnote->write_commit (1);
                            }
                        }
                    }
                }
                else
                {
                    // Note off.
                    if (n < 36)
                    {
                        if ((f & 4) && (n >= 0) && (n < 24))
                        {
                            // Stop disable, sent to model thread
                            if (_qmidi->write_avail () >= 3)
                            {
                            // send group and set off
                            _qmidi->write (0, 0xB0 | c);
                            _qmidi->write (1, MIDICTL_IFELM);
                            _qmidi->write (2, 0x50 | g);
                            _qmidi->write_commit (3);
                            // send stop number
                            _qmidi->write (0, 0xB0 | c);
                            _qmidi->write (1, MIDICTL_IFELM);
                            _qmidi->write (2, n);
                            _qmidi->write_commit (3);
                            }
                        }
                    }
                    else if (n <= 96)
                    {
                        if (m)
                        {
                            if (_qnote->write_avail () > 0)
                            {
                                _qnote->write (0, ((n - 36) << 8) | m);
                                    _qnote->write_commit (1);
                            }
                        }
                    }
                }
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                p = E->data.control.param;
                v = E->data.control.value;
                debug("cc c=0x%02x p=0x%02x v=0x%02x", c, p, v);

                switch (p)
                {
                    case MIDICTL_HOLD:
                    // Hold pedal.
                    if (m & HOLD_MASK)
                    {
                        v = (v > 63) ? 9 : 8;
                            if (_qnote->write_avail () > 0)
                            {
                                _qnote->write (0, (v << 24) | (m << 16));
                                    _qnote->write_commit (1);
                            }
                    }
                    break;

                    case MIDICTL_ASOFF:
                    // All sound off, accepted on control channels only.
                    // Clears all keyboards, including held notes.
                    if (f & 4)
                    {
                        if (_qnote->write_avail () > 0)
                        {
                            _qnote->write (0, (2 << 24) | ( ALL_MASK << 16) | ALL_MASK);
                            _qnote->write_commit (1);
                        }
                    }
                    break;

                    case MIDICTL_ANOFF:
                    // All notes off, accepted on channels controlling
                    // a keyboard. Does not clear held notes.
                    if (m)
                    {
                        if (_qnote->write_avail () > 0)
                        {
                            _qnote->write (0, (2 << 24) | (m << 16) | m);
                                _qnote->write_commit (1);
                        }
                    }
                    break;

                    case MIDICTL_DAZIM:
                    case MIDICTL_DWIDT:
                    case MIDICTL_DDIRE:
                    case MIDICTL_DREFL:
                    case MIDICTL_DREVB:
                    // Divison audio params, sent to model thread if controlling
                    // a keyboard.
                    if (m)
                    {
                        if (_qmidi->write_avail () >= 3)
                        {
                        // send the division channel rather than midi channel
                        //_qmidi->write (0, 0xB0 | c);
                        switch (m & 0x0F)
                        {
                        case 1: // III
                            _qmidi->write (0, 0xB0 | 0);
                            break;
                        case 2: // II
                            _qmidi->write (0, 0xB0 | 1);
                            break;
                        case 4: // I
                            _qmidi->write (0, 0xB0 | 2);
                            break;
                        case 8: // P
                            _qmidi->write (0, 0xB0 | 3);
                            break;
                        }
                        _qmidi->write (1, p);
                        _qmidi->write (2, v);
                        _qmidi->write_commit (3);
                        }
                    }
                    break;

                    case MIDICTL_MAVOL:
                    case MIDICTL_RDELY:
                    case MIDICTL_RTIME:
                    case MIDICTL_RPOSI:
                    case MIDICTL_PNEXT:
                    case MIDICTL_PPREV:
                    case MIDICTL_PSTOR:
                    case MIDICTL_CANCL:
                    case MIDICTL_TUTTI:
                    case MIDICTL_TRNSP:
                    case MIDICTL_BANK:
                    case MIDICTL_IFELM:
                    // Program bank selection, audio param or stop control, sent
                    // to model thread if on control-enabled channel.
                    if (f & 4)
                    {
                        if (_qmidi->write_avail () >= 3)
                        {
                        _qmidi->write (0, 0xB0 | c);
                        _qmidi->write (1, p);
                        _qmidi->write (2, v);
                        _qmidi->write_commit (3);
                        }
                    }
                    break;

                    case MIDICTL_SWELL:
                    case MIDICTL_TFREQ:
                    case MIDICTL_TMODD:
                    // Per-division performance controls, sent to model
                            // thread if on a channel that controls a division.
                    if (f & 2)
                    {
                        if (_qmidi->write_avail () >= 3)
                        {
                        _qmidi->write (0, 0xB0 | c);
                        _qmidi->write (1, p);
                        _qmidi->write (2, v);
                        _qmidi->write_commit (3);
                        }
                    }
                    break;

                    case MIDICTL_PRCAL:
                        // Program change sent to model thread
                        // if on control-enabled channel.
                        if (f & 4)
                        {
                            if (_qmidi->write_avail () >= 3)
                            {
                            _qmidi->write (0, 0xC0);
                            _qmidi->write (1, v);
                            _qmidi->write (2, 0);
                            _qmidi->write_commit (3);
                            }
                        }
                    }
                    break;

            case SND_SEQ_EVENT_PGMCHANGE:
                    // Program change sent to model thread
                    // if on control-enabled channel.
                if (f & 4)
                {
                    if (_qmidi->write_avail () >= 3)
                    {
                    _qmidi->write (0, 0xC0);
                    _qmidi->write (1, E->data.control.value);
                    _qmidi->write (2, 0);
                    _qmidi->write_commit (3);
                    }
                }
                break;

            case SND_SEQ_EVENT_USR0:
                // User event, terminates this trhead if we sent it.
                if (E->source.client == _client) return;
        }
    }
}


#else

// Native Windows MIDI input using WinMM. Incoming short messages are copied
// from the multimedia callback into a bounded SPSC queue; all Aeolus queue
// and model processing happens on the Imidi worker thread.
#include <mmsystem.h>

Imidi::Imidi (Lfq_u32 *qnote, Lfq_u8 *qmidi, uint16_t *midimap, const char *appname) :
    A_thread ("Imidi"),
    _qnote (qnote),
    _qmidi (qmidi),
    _midimap (midimap),
    _appname (appname),
    _client (0),
    _ipport (0),
    _opport (0),
    _win_input (0),
    _win_running (false),
    _win_write (0),
    _win_read (0)
{
}

Imidi::~Imidi (void)
{
    terminate ();
}

void CALLBACK Imidi::midi_callback (HMIDIIN, UINT message, DWORD_PTR instance,
                                    DWORD_PTR param1, DWORD_PTR)
{
    Imidi *self = (Imidi *) instance;
    if (!self || message != MIM_DATA) return;

    unsigned write = self->_win_write.load (std::memory_order_relaxed);
    unsigned next = (write + 1) & 255;
    if (next == self->_win_read.load (std::memory_order_acquire)) return;
    self->_win_messages [write] = (DWORD) param1;
    self->_win_write.store (next, std::memory_order_release);
}

void Imidi::terminate (void)
{
    _win_running.store (false, std::memory_order_release);
}

void Imidi::process_win_message (DWORD message)
{
    unsigned status = message & 0xFF;
    unsigned d1 = (message >> 8) & 0x7F;
    unsigned d2 = (message >> 16) & 0x7F;
    unsigned type = status & 0xF0;
    unsigned channel = status & 0x0F;
    unsigned map = _midimap [channel];
    unsigned keyboard = map & 127;
    unsigned flags = (map >> 12) & 7;

    if (type == 0x90 || type == 0x80)
    {
        bool on = (type == 0x90 && d2 != 0);
        unsigned note = d1;
        unsigned velocity = d2;
        unsigned group = 0;
        switch (keyboard & 0x0F)
        {
            case 0x01: group = 0; break;
            case 0x02: group = 1; break;
            case 0x04: group = 2; break;
            case 0x08: group = 3; break;
            case 0x0F: group = 4; break;
        }
        if (note < 36)
        {
            if ((flags & 4) && note < 24 && _qmidi->write_avail () >= 6)
            {
                _qmidi->write (0, 0xB0 | channel);
                _qmidi->write (1, MIDICTL_IFELM);
                _qmidi->write (2, (on ? 0x60 : 0x50) | group);
                _qmidi->write_commit (3);
                _qmidi->write (0, 0xB0 | channel);
                _qmidi->write (1, MIDICTL_IFELM);
                _qmidi->write (2, note);
                _qmidi->write_commit (3);
            }
            else if (on && (flags & 4) && note >= 24 && note < 34 &&
                     _qmidi->write_avail () >= 3)
            {
                _qmidi->write (0, 0x90);
                _qmidi->write (1, note);
                _qmidi->write (2, velocity);
                _qmidi->write_commit (3);
            }
        }
        else if (note <= 96 && keyboard && _qnote->write_avail () > 0)
        {
            _qnote->write (0, (on ? (1u << 24) : 0) |
                           ((note - 36) << 8) | keyboard);
            _qnote->write_commit (1);
        }
    }
    else if (type == 0xB0)
    {
        unsigned p = d1, value = d2;
        if (p == MIDICTL_HOLD && (keyboard & HOLD_MASK) &&
            _qnote->write_avail () > 0)
        {
            unsigned hold = value > 63 ? 9 : 8;
            _qnote->write (0, (hold << 24) | (keyboard << 16));
            _qnote->write_commit (1);
        }
        else if (p == MIDICTL_ASOFF && (flags & 4) &&
                 _qnote->write_avail () > 0)
        {
            _qnote->write (0, (2u << 24) | (ALL_MASK << 16) | ALL_MASK);
            _qnote->write_commit (1);
        }
        else if (p == MIDICTL_ANOFF && keyboard &&
                 _qnote->write_avail () > 0)
        {
            _qnote->write (0, (2u << 24) | (keyboard << 16) | keyboard);
            _qnote->write_commit (1);
        }
        else if ((flags & 4) && _qmidi->write_avail () >= 3)
        {
            _qmidi->write (0, 0xB0 | channel);
            _qmidi->write (1, p);
            _qmidi->write (2, value);
            _qmidi->write_commit (3);
        }
        else if ((flags & 2) && keyboard && _qmidi->write_avail () >= 3)
        {
            unsigned division_channel = (keyboard & 1) ? 0 :
                                        (keyboard & 2) ? 1 :
                                        (keyboard & 4) ? 2 : 3;
            _qmidi->write (0, 0xB0 | division_channel);
            _qmidi->write (1, p);
            _qmidi->write (2, value);
            _qmidi->write_commit (3);
        }
    }
    else if (type == 0xC0 && (flags & 4) && _qmidi->write_avail () >= 3)
    {
        _qmidi->write (0, 0xC0);
        _qmidi->write (1, d1);
        _qmidi->write (2, 0);
        _qmidi->write_commit (3);
    }
}

void Imidi::thr_main (void)
{
    M_midi_info *info = new M_midi_info ();
    info->_client = 0;
    info->_ipport = 0;
    info->_opport = 0;
    info->_backend = 0;
    memcpy (info->_chbits, _midimap, 16 * sizeof (uint16_t));
    send_event (TO_MODEL, info);

    _win_running.store (true, std::memory_order_release);
    UINT count = midiInGetNumDevs ();
    if (count == 0)
        fprintf (stderr, "Aeolus: no Windows MIDI input devices found.\\n");
    else
    {
        for (UINT i = 0; i < count; ++i)
        {
            MIDIINCAPSW caps;
            if (midiInGetDevCapsW (i, &caps, sizeof (caps)) == MMSYSERR_NOERROR)
                fwprintf (stderr, L"Aeolus MIDI input %u: %ls\\n", i, caps.szPname);
        }
        MMRESULT result = midiInOpen (&_win_input, 0,
            (DWORD_PTR) &Imidi::midi_callback, (DWORD_PTR) this, CALLBACK_FUNCTION);
        if (result == MMSYSERR_NOERROR)
        {
            if (midiInStart (_win_input) != MMSYSERR_NOERROR)
            {
                midiInClose (_win_input);
                _win_input = 0;
            }
        }
        else
        {
            fprintf (stderr, "Aeolus: could not open default Windows MIDI input (error %u).\\n",
                     (unsigned) result);
            _win_input = 0;
        }
    }

    while (_win_running.load (std::memory_order_acquire))
    {
        unsigned read = _win_read.load (std::memory_order_relaxed);
        unsigned write = _win_write.load (std::memory_order_acquire);
        if (read != write)
        {
            DWORD msg = _win_messages [read];
            _win_read.store ((read + 1) & 255, std::memory_order_release);
            process_win_message (msg);
        }
        else Sleep (2);
    }

    if (_win_input)
    {
        midiInStop (_win_input);
        midiInReset (_win_input);
        midiInClose (_win_input);
        _win_input = 0;
    }
    send_event (EV_EXIT, 1);
}

#endif // __linux__
