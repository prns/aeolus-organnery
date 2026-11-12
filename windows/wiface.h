#ifndef __WIFACE_H
#define __WIFACE_H

#include "../source/iface.h"
#include <windows.h>
#include <vector>

class Wiface : public Iface
{
public:
    Wiface(int ac, char *av[]);
    virtual ~Wiface();
    virtual void stop(void);
private:
    struct StopItem { RECT rect; int group; int index; };
    static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT dispatch(HWND, UINT, WPARAM, LPARAM);
    virtual void thr_main(void);
    void handle_message(ITC_mesg *);
    void paint(HDC);
    void layout(void);
    void toggle_at(int, int);
    HWND _window;
    bool _stop;
    bool _ready;
    M_ifc_init *_initdata;
    std::vector<StopItem> _items;
    uint32_t _states[NGROUP];
    int _width, _height;
};
#endif
