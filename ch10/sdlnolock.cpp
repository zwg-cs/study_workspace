#include <iostream>
#include <SDL.h>

#undef main

extern "C"
{
#include <libavformat/avformat.h>
}

int number = 0;
int thread_work(void *arg)
{
    int count = 0;
    while (++count < 10)
    {
        av_log(nullptr, AV_LOG_INFO, "Thread begin deal, the number is %d\n", number);
        SDL_Delay(100);
        av_log(nullptr, AV_LOG_INFO, "Thread end deal, the number is %d\n", number);
        SDL_Delay(100);
    }
    return 1;
}

int main(int argc, char **argv)
{
    SDL_Thread *sdl_thread = SDL_CreateThread(thread_work, "thread_work", nullptr);
    if (!sdl_thread)
    {
        av_log(nullptr, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    int count = 0;
    while (++count < 100)
    {
        number = count;
        SDL_Delay(30);
    }
    // 线程结束标志
    int finish_status;
    SDL_WaitThread(sdl_thread, &finish_status);
    av_log(NULL, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
    return 0;
}