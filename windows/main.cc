// Windows console entry point for the first runnable Aeolus port.
// The original X11 GUI and native Windows MIDI backend are not part of this
// bootstrap executable yet.

#include <clthreads.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../source/audio.h"
#include "../source/imidi.h"
#include "../source/model.h"
#include "../source/slave.h"
#include "../source/tiface.h"
#include "wasapi_audio_output.h"

// Global debug switch declared by source/global.h.
bool x_opt = false;

static AeolusWasapiOutput *g_output = 0;

static BOOL WINAPI console_control(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT)
    {
        if (g_output) g_output->stop();
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    const char *appname = "aeolus";
    const char *stops = "stops";
    const char *instr = "Aeolus";
    const char *waves = "waves";
    bool user_home = false;
    bool bform = false;
    bool check_data = false;

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-u")) user_home = true;
        else if (!strcmp(argv[i], "-B")) bform = true;
        else if (!strcmp(argv[i], "--check-data")) check_data = true;
        else if ((!strcmp(argv[i], "-N") || !strcmp(argv[i], "-S") ||
                  !strcmp(argv[i], "-I") || !strcmp(argv[i], "-W")) && i + 1 < argc)
        {
            const char *value = argv[++i];
            if (!strcmp(argv[i - 1], "-N")) appname = value;
            else if (!strcmp(argv[i - 1], "-S")) stops = value;
            else if (!strcmp(argv[i - 1], "-I")) instr = value;
            else waves = value;
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            printf("Aeolus Windows bootstrap\n"
                   "Usage: aeolus.exe [-u] [-B] [-N name] [-S stops] [-I instrument] [-W waves]\n"
                   "This build uses WASAPI shared-mode audio and the text interface.\n"
                   "Windows MIDI and the original graphical interface are not implemented yet.\n");
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return 2;
        }
    }

    // Resolve bundled stops/waves relative to the executable, not the
    // shell's current working directory. This makes the extracted package
    // runnable from a shortcut or another working directory.
    wchar_t module_path[32768];
    DWORD module_length = GetModuleFileNameW(NULL, module_path,
                                             (DWORD)(sizeof(module_path) / sizeof(module_path[0])));
    if (module_length == 0 ||
        module_length >= sizeof(module_path) / sizeof(module_path[0]))
    {
        fprintf(stderr, "Aeolus: could not determine the executable directory.\n");
        return 1;
    }
    wchar_t *last_separator = wcsrchr(module_path, L'\\');
    wchar_t *last_forward = wcsrchr(module_path, L'/');
    if (last_forward && (!last_separator || last_forward > last_separator))
        last_separator = last_forward;
    if (!last_separator)
    {
        fprintf(stderr, "Aeolus: executable path has no directory.\n");
        return 1;
    }
    *last_separator = L'\0';
    if (!SetCurrentDirectoryW(module_path))
    {
        fprintf(stderr, "Aeolus: could not set the executable directory as working directory.\n");
        return 1;
    }

    if (check_data)
    {
        DWORD definition_attr = GetFileAttributesW(L"stops\\\\Aeolus\\\\definition");
        DWORD waves_attr = GetFileAttributesW(L"stops\\\\waves");
        if (definition_attr == INVALID_FILE_ATTRIBUTES ||
            (definition_attr & FILE_ATTRIBUTE_DIRECTORY) ||
            waves_attr == INVALID_FILE_ATTRIBUTES ||
            !(waves_attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            fprintf(stderr, "Aeolus: bundled instrument data is missing beside the executable.\\n");
            return 1;
        }
        printf("Aeolus package data found beside executable.\\n");
        return 0;
    }

    Lfq_u32 note_queue(256);
    Lfq_u32 comm_queue(256);
    Lfq_u8 midi_queue(1024);
    ITC_ctrl itcc;

    Audio *audio = new Audio(appname, &note_queue, &comm_queue);
    Imidi *imidi = new Imidi(&note_queue, &midi_queue, audio->midimap(), appname);
    Model *model = new Model(&comm_queue, &midi_queue, audio->midimap(),
                             audio->appname(), stops, instr, waves, user_home);
    Slave *slave = new Slave();
    Iface *iface = new Tiface(argc, argv);
    AeolusWasapiOutput output;
    g_output = &output;

    if (!output.open(audio, bform))
    {
        fprintf(stderr, "Aeolus: could not open the default WASAPI output device.\n");
        delete iface; delete slave; delete model; delete imidi; delete audio;
        g_output = 0;
        return 1;
    }

    ITC_ctrl::connect(audio, EV_EXIT, &itcc, EV_EXIT);
    ITC_ctrl::connect(audio, EV_QMIDI, model, EV_QMIDI);
    ITC_ctrl::connect(audio, TO_MODEL, model, FM_AUDIO);
    ITC_ctrl::connect(imidi, EV_EXIT, &itcc, EV_EXIT);
    ITC_ctrl::connect(imidi, EV_QMIDI, model, EV_QMIDI);
    ITC_ctrl::connect(imidi, TO_MODEL, model, FM_IMIDI);
    ITC_ctrl::connect(model, EV_EXIT, &itcc, EV_EXIT);
    ITC_ctrl::connect(model, TO_AUDIO, audio, FM_MODEL);
    ITC_ctrl::connect(model, TO_SLAVE, slave, FM_MODEL);
    ITC_ctrl::connect(model, TO_IFACE, iface, FM_MODEL);
    ITC_ctrl::connect(slave, EV_EXIT, &itcc, EV_EXIT);
    ITC_ctrl::connect(slave, TO_AUDIO, audio, FM_SLAVE);
    ITC_ctrl::connect(slave, TO_MODEL, model, FM_SLAVE);
    ITC_ctrl::connect(iface, EV_EXIT, &itcc, EV_EXIT);
    ITC_ctrl::connect(iface, TO_MODEL, model, FM_IFACE);

    SetConsoleCtrlHandler(console_control, TRUE);
    imidi->thr_start(SCHED_OTHER, 0, 0);
    model->thr_start(SCHED_OTHER, 0, 0);
    slave->thr_start(SCHED_OTHER, 0, 0);
    iface->thr_start(SCHED_OTHER, 0, 0);
    audio->start();

    printf("Aeolus Windows bootstrap started. Press Ctrl+C to exit.\n");
    bool audio_ok = output.run();

    output.stop();
    imidi->terminate();
    model->terminate();
    slave->terminate();
    iface->terminate();

    for (int i = 0; i < 4; ++i) itcc.get_event(1 << EV_EXIT);

    SetConsoleCtrlHandler(console_control, FALSE);
    output.close();
    g_output = 0;

    delete iface;
    delete slave;
    delete model;
    delete imidi;
    delete audio;
    return audio_ok ? 0 : 1;
}
