#pragma once
#include <windows.h>

// A one-pixel window owns the cursor at the capture point. Raw input still
// reports device motion while the desktop pointer itself cannot move.
class MouseCapture {
    HWND target_{}, sink_{};
    RECT oldClip_{};
    POINT oldPoint_{};
    int dx_{}, dy_{};
    static LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
        auto self = reinterpret_cast<MouseCapture*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if (msg == WM_SETCURSOR) { SetCursor(nullptr); return TRUE; }
        if (msg == WM_INPUT && self) {
            RAWINPUT raw{}; UINT size=sizeof(raw);
            if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))==sizeof(raw) && raw.header.dwType==RIM_TYPEMOUSE && !(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)) {
                self->dx_ += raw.data.mouse.lLastX; self->dy_ += raw.data.mouse.lLastY;
            }
        }
        return DefWindowProcW(w,msg,wp,lp);
    }
public:
    ~MouseCapture(){ release(); }
    void acquire(){
        if(sink_) return;
        target_=GetForegroundWindow(); RECT r{};
        if(!target_ || !GetClientRect(target_,&r) || r.right<2 || r.bottom<2) return;
        POINT center{r.right/2,r.bottom/2};
        if(!ClientToScreen(target_,&center)||!GetCursorPos(&oldPoint_)||!GetClipCursor(&oldClip_)) return;
        WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"RaycastMouseSink";
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return;
        sink_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,wc.lpszClassName,L"",WS_POPUP,center.x,center.y,1,1,nullptr,nullptr,wc.hInstance,nullptr);
        if(!sink_) return;
        SetWindowLongPtrW(sink_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
        SetLayeredWindowAttributes(sink_,0,1,LWA_ALPHA);
        RAWINPUTDEVICE dev{1,2,RIDEV_INPUTSINK,sink_}; RECT pin{center.x,center.y,center.x+1,center.y+1};
        if(!RegisterRawInputDevices(&dev,1,sizeof(dev)) || !ClipCursor(&pin)){release(); return;}
        ShowWindow(sink_,SW_SHOWNOACTIVATE); SetCursorPos(center.x,center.y);
    }
    void release(){
        if(!sink_)return;
        RAWINPUTDEVICE dev{1,2,RIDEV_REMOVE,nullptr}; RegisterRawInputDevices(&dev,1,sizeof(dev));
        ClipCursor(&oldClip_); DestroyWindow(sink_); sink_=nullptr; SetCursorPos(oldPoint_.x,oldPoint_.y); dx_=dy_=0;
    }
    void poll(int& x,int& y){
        if(!sink_)return;
        if(GetForegroundWindow()!=target_){release();return;}
        MSG msg{}; while(PeekMessageW(&msg,sink_,0,0,PM_REMOVE)) DispatchMessageW(&msg);
        x=dx_; y=dy_; dx_=dy_=0;
    }
};
