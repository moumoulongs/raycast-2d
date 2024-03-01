#pragma once
#include<windows.h>
#include<iostream>

// //双缓冲
// int active = 0;
// COORD coord = {0,0};
// DWORD bytes = 0;
// //创建新的控制台缓冲区
// HANDLE buf0 = CreateConsoleScreenBuffer(
//     GENERIC_WRITE,//定义进程可以往缓冲区写数据
//     FILE_SHARE_WRITE,//定义缓冲区可共享写权限
//     NULL,
//     CONSOLE_TEXTMODE_BUFFER,
//     NULL
// );
// HANDLE buf1 = CreateConsoleScreenBuffer(
//     GENERIC_WRITE,//定义进程可以往缓冲区写数据
//     FILE_SHARE_WRITE,//定义缓冲区可共享写权限
//     NULL,
//     CONSOLE_TEXTMODE_BUFFER,
//     NULL
// );

constexpr int dW = 9, dH = 9; // 字体大小

//窗口大小
int WIDTH = (GetSystemMetrics(SM_CXSCREEN) - dW) / dW + 1;
int HEIGHT = (GetSystemMetrics(SM_CYSCREEN) - dH) / dH;

class Screen {
    public:

        Screen() {
            Setup();
            Clear();
        }
        //画布清除
        void Clear() {
            for(int i = 0; i < HEIGHT; i++) {
                for(int j = 0; j < WIDTH; j++) {
                    if(i >= HEIGHT/2) canvas[i * WIDTH + j] = 15;
                    else canvas[i * WIDTH + j] = 0;
                }
            }
        }
        //画点
        void DrawPoint(int x, int y, byte luminuns) { //(x,y)处亮度为luminuns的点
            if(x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) {
                return;
            }else{
                canvas[y * WIDTH + x] = luminuns;
            }
        }
        //画线 bresenham
        void DrawLine(int x0, int y0, int x1, int y1, byte luminuns) {
            if(x0 < 0 || y0 < 0 || x0 >= WIDTH || y0 >= HEIGHT) return;
            if(x1 < 0 || y1 < 0 || x1 >= WIDTH || y1 >= HEIGHT) return;
            int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int erro = (dx > dy ? dx : -dy) / 2;
        
            while(canvas[y0 * WIDTH + x0] = luminuns, x0 != x1 || y0 != y1){
                int e2 = erro;
                if(e2 > -dx) { erro -= dy; x0 += sx;}
                if(e2 <  dy) { erro += dx; y0 += sy;}
            }
        }
        //将画布渲染至屏幕
        void Show() {
            char* frame=new char[WIDTH*HEIGHT];
            for(int i = 0; i < HEIGHT - 1; i++) {
                frame[WIDTH * i + WIDTH - 1] = '\n';//除了最后一行填充换行符
            }
            frame[WIDTH * HEIGHT - 1] = '\0';//字符串末尾

            //将画布与边界投影到frame上
            for(int i = 0; i < HEIGHT; i++) {
                for(int j = 0; j < WIDTH-1; j++) {
                    frame[i * WIDTH + j] = brightness(canvas[WIDTH*i+j]);
                }
            }
            for (int i = 0; i < HEIGHT; ++i) {
                frame[WIDTH * i] = '@';
                frame[WIDTH * i + WIDTH - 2] = '@';
            }
            for (int j = 0; j < WIDTH-1; ++j) {
                frame[j] = '@';
                frame[WIDTH*(HEIGHT-1)+j] = '@';
            }
            FillScreenWithString(frame);
            delete[] frame;
        }

        int Height() {
            return HEIGHT;
        }

        int Width() {
            return WIDTH;
        }

        ~Screen() {
            delete[] canvas;
        }
    private:
        byte *canvas = new byte[WIDTH * HEIGHT];

        void Setup();

        void FillScreenWithString(const char *frame);

        char brightness(byte n) {
            char s[] = " .,^:-+abcdwf$&%#@";
            return s[n*18/256];
        }
};

void Screen::FillScreenWithString(const char *frame) {
    COORD coord = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    fputs(frame,stdout);
}

void Screen::Setup() {
    // 获取标准输出的句柄
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    //全屏
    SetConsoleDisplayMode(hConsole, CONSOLE_FULLSCREEN_MODE, nullptr);
    

    // 设置字体大小
    CONSOLE_FONT_INFOEX cf = {0};
    cf.cbSize = sizeof cf;
    cf.dwFontSize.X = dW;
    cf.dwFontSize.Y = dH;
    wcscpy_s(cf.FaceName, L"Terminal");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), 0, &cf);
    //光标隐藏
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = false; // 隐藏光标
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);

    HWND console = GetConsoleWindow();
    //设置控制台屏幕缓冲区大小
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hstdout, &csbi);
    csbi.dwSize.X = csbi.dwMaximumWindowSize.X;
    csbi.dwSize.Y = csbi.dwMaximumWindowSize.Y;
    SetConsoleScreenBufferSize(hstdout, csbi.dwSize);

    //设置为全屏模式
    SetConsoleDisplayMode(hstdout, CONSOLE_FULLSCREEN_MODE, 0);
    //移动窗口Y
    MoveWindow(console, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), TRUE);
    //隐藏光标

}