#include <iostream>
#include <SDL.h>

#undef main

extern "C"
{
#include <libavformat/avformat.h>
}
// 声明一个互斥锁，防止线程间同时操作某个变量
SDL_mutex *sdl_lock = nullptr;
int number; // 声明一个整型变量

int thread_work1(void *arg)
{
    int count = 0;
    while (++count < 10)
    {
        SDL_LockMutex(sdl_lock); // 对互斥锁加锁
        av_log(nullptr, AV_LOG_INFO, "First thread begin deal, the number is %d\n", number);
        SDL_Delay(50);
        av_log(nullptr, AV_LOG_INFO, "First thread end deal, the number is %d\n", number);
        SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
        SDL_Delay(50);
    }
    return 1;
}

int thread_work2(void *arg)
{
    int count = 0;
    while (++count < 10)
    {
        SDL_LockMutex(sdl_lock); // 对互斥锁加锁
        av_log(nullptr, AV_LOG_INFO, "Second thread begin deal, the number is %d\n", number);
        SDL_Delay(50);
        av_log(nullptr, AV_LOG_INFO, "Second thread end deal, the number is %d\n", number);
        SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
        SDL_Delay(50);
    }
    return 1;
}

int main(int argc, char **argv)
{
    sdl_lock = SDL_CreateMutex(); // 创建互斥锁

    SDL_Thread *sdl_thread1 = SDL_CreateThread(thread_work1, "thread_work", NULL);
    if (!sdl_thread1)
    {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    SDL_Delay(10);
    SDL_Thread *sdl_thread2 = SDL_CreateThread(thread_work2, "thread_work", NULL);
    if (!sdl_thread2)
    {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }

    int count = 0;

    while (++count < 100)
    {
        SDL_LockMutex(sdl_lock);
        number = count;
        SDL_Delay(10);
        SDL_UnlockMutex(sdl_lock);
        SDL_Delay(10);
    }

    int finish_status;
    SDL_WaitThread(sdl_thread1, &finish_status);
    av_log(nullptr, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
    SDL_WaitThread(sdl_thread2, &finish_status);
    av_log(nullptr, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
    SDL_DestroyMutex(sdl_lock); // 销毁互斥锁
    return 0;
}