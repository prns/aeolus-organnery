#include "wiface.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

Wiface::Wiface(int, char **) :
    _window(0), _stop(false), _ready(false), _initdata(0),
    _width(1100), _height(760)
{
    memset(_states, 0, sizeof(_states));
}
Wiface::~Wiface()
{
    if (_initdata) _initdata->recover();
}
void Wiface::stop(void) { _stop = true; }

LRESULT CALLBACK Wiface::window_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    Wiface *self = reinterpret_cast<Wiface *>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        CREATESTRUCTW *cs = reinterpret_cast<CREATESTRUCTW *>(lp);
        self = static_cast<Wiface *>(cs->lpCreateParams);
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->dispatch(w, m, wp, lp) : DefWindowProcW(w, m, wp, lp);
}
LRESULT Wiface::dispatch(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps);
        paint(dc); EndPaint(w, &ps); return 0;
    }
    case WM_LBUTTONUP:
        toggle_at(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_SIZE:
        InvalidateRect(w, 0, TRUE); return 0;
    case WM_CLOSE:
        _stop = true; DestroyWindow(w); return 0;
    case WM_DESTROY:
        _window = 0; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}
void Wiface::layout(void)
{
    _items.clear();
    if (!_initdata) return;
    int cols = (_initdata->_ngroup > 4) ? 2 : 1;
    int colw = (_width - 48) / cols;
    for (int g = 0; g < _initdata->_ngroup; ++g) {
        int col = g / 4, row = g % 4;
        int x = 18 + col * colw, y = 56 + row * 166;
        int n = _initdata->_groupd[g]._nifelm;
        for (int i = 0; i < n && i < 32; ++i) {
            StopItem item;
            int r = i / 4, c = i % 4;
            item.rect.left = x + c * ((colw - 28) / 4);
            item.rect.top = y + 24 + r * 17;
            item.rect.right = item.rect.left + ((colw - 28) / 4) - 5;
            item.rect.bottom = item.rect.top + 15;
            item.group = g; item.index = i;
            _items.push_back(item);
        }
    }
}
void Wiface::paint(HDC dc)
{
    RECT client; GetClientRect(_window, &client);
    HBRUSH bg = CreateSolidBrush(RGB(30, 34, 39));
    FillRect(dc, &client, bg); DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 235, 218));
    HFONT title = CreateFontW(24,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HFONT old = (HFONT)SelectObject(dc, title);
    const char *name = _initdata ? _initdata->_instr : "Aeolus";
    TextOutA(dc, 18, 14, name, (int)strlen(name));
    SelectObject(dc, old); DeleteObject(title);
    if (!_initdata) { TextOutA(dc, 20, 60, "Starting organ engine...", 24); return; }
    HFONT font = CreateFontW(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    old = (HFONT)SelectObject(dc, font);
    int cols = (_initdata->_ngroup > 4) ? 2 : 1;
    int colw = (_width - 48) / cols;
    for (int g = 0; g < _initdata->_ngroup; ++g) {
        int col=g/4, row=g%4, x=18+col*colw, y=56+row*166;
        SetTextColor(dc, RGB(219,190,125));
        const char *label=_initdata->_groupd[g]._label;
        TextOutA(dc,x,y,label,(int)strlen(label));
    }
    for (size_t k=0;k<_items.size();++k) {
        const StopItem &it=_items[k];
        const char *label=_initdata->_groupd[it.group]._ifelmd[it.index]._mnemo;
        bool on=(_states[it.group] & (1u<<it.index)) != 0;
        HBRUSH br=CreateSolidBrush(on?RGB(145,105,45):RGB(62,68,74));
        FillRect(dc,&it.rect,br); DeleteObject(br);
        FrameRect(dc,&it.rect,(HBRUSH)GetStockObject(GRAY_BRUSH));
        SetTextColor(dc,on?RGB(255,250,225):RGB(205,210,212));
        RECT r=it.rect; r.left+=4;
        DrawTextA(dc,label,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }
    SelectObject(dc,old); DeleteObject(font);
}
void Wiface::toggle_at(int x,int y)
{
    for(size_t k=0;k<_items.size();++k) {
        const StopItem &it=_items[k];
        if(PtInRect(&it.rect,POINT{x,y})) {
            send_event(TO_MODEL,new M_ifc_ifelm(MT_IFC_ELXOR,it.group,it.index));
            return;
        }
    }
}
void Wiface::handle_message(ITC_mesg *m)
{
    switch(m->type()) {
    case MT_IFC_INIT:
        if(_initdata) _initdata->recover();
        _initdata=(M_ifc_init *)m; m=0;
        layout(); if(_window) InvalidateRect(_window,0,TRUE);
        break;
    case MT_IFC_ELSET: {
        M_ifc_ifelm *x=(M_ifc_ifelm *)m;
        if(x->_group>=0&&x->_group<NGROUP&&x->_ifelm>=0&&x->_ifelm<32)
            _states[x->_group]|=(1u<<x->_ifelm);
        if(_window) InvalidateRect(_window,0,FALSE);
        break;
    }
    case MT_IFC_ELCLR: {
        M_ifc_ifelm *x=(M_ifc_ifelm *)m;
        if(x->_group>=0&&x->_group<NGROUP&&x->_ifelm>=0&&x->_ifelm<32)
            _states[x->_group]&=~(1u<<x->_ifelm);
        if(_window) InvalidateRect(_window,0,FALSE);
        break;
    }
    case MT_IFC_GRCLR: {
        M_ifc_ifelm *x=(M_ifc_ifelm *)m;
        if(x->_group>=0&&x->_group<NGROUP) _states[x->_group]=0;
        if(_window) InvalidateRect(_window,0,FALSE);
        break;
    }
    default: break;
    }
    if(m) m->recover();
}
void Wiface::thr_main(void)
{
    _stop=false;
    WNDCLASSW wc; memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=window_proc; wc.hInstance=GetModuleHandleW(0);
    wc.lpszClassName=L"AeolusNativeOrganConsole";
    wc.hCursor=LoadCursorW(0,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    _window=CreateWindowExW(0,wc.lpszClassName,L"Aeolus - Organ Console",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
        _width,_height,0,0,wc.hInstance,this);
    if(!_window) { send_event(EV_EXIT,1); return; }
    ShowWindow(_window,SW_SHOW); UpdateWindow(_window);
    set_time(0); inc_time(10000);
    while(!_stop) {
        MSG msg;
        while(PeekMessageW(&msg,0,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT) { _stop=true; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if(_stop) break;
        switch(get_event_timed()) {
        case FM_MODEL: handle_message(get_message()); break;
        case EV_EXIT: _stop=true; break;
        case EV_TIME: inc_time(10000); break;
        }
    }
    if(_window) { DestroyWindow(_window); _window=0; }
    send_event(EV_EXIT,1);
}
