#include <iostream>
#include <SDL.h>

#undef main
extern "C"
{
#include <libavutil/avutil.h>
}
// 什么互斥锁
SDL_mutex *sdl_lock = nullptr;
// 声明一个信号
SDL_cond *sdl_signal = nullptr;
// 整形变量
int number;

// 分线程任务处理
int thread_work1(void *arg)
{
    int count = 0;
    SDL_LockMutex(sdl_lock); // 对互斥锁加锁
    while (++count < 10)
    {
        av_log(NULL, AV_LOG_INFO, "First thread begin deal, the number is %d\n", number);
        SDL_Delay(100); // 延迟若干时间，单位毫秒
        av_log(NULL, AV_LOG_INFO, "First thread end deal, the number is %d\n", number);
        // 释放锁资源（解锁），并等待响应信号。收到响应信号之后，会重新加锁
        SDL_CondWait(sdl_signal, sdl_lock);
    }
    SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
    return -1;
}

int thread_work2(void *arg)
{
    int count = 0;
    SDL_LockMutex(sdl_lock); // 对互斥锁加锁
    while (++count < 10)
    {
        av_log(NULL, AV_LOG_INFO, "Second thread begin deal, the number is %d\n", number);
        SDL_Delay(50); // 延迟若干时间，单位毫秒
        av_log(NULL, AV_LOG_INFO, "Second thread end deal, the number is %d\n", number);
        // 释放锁资源（解锁），并等待响应信号。收到响应信号之后，会重新加锁
        SDL_CondWait(sdl_signal, sdl_lock);
    }
    SDL_UnlockMutex(sdl_lock);
    return 1; // 返回线程的结束标志
}
int main(int argc, char **argv)
{
    sdl_lock = SDL_CreateMutex();  // 创建互斥锁
    sdl_signal = SDL_CreateCond(); // 创建信号量（条件量）
    SDL_Thread *sdl_thread1 = SDL_CreateThread(thread_work1, "thread_work1", nullptr);
    if (!sdl_thread1)
    {
        av_log(nullptr, AV_LOG_INFO, "sdl create thread occur error\n");
        return -1;
    }
    SDL_Delay(10);

    SDL_Thread *sdl_thread2 = SDL_CreateThread(thread_work2, "thread_work2", nullptr);
    if (!sdl_thread2)
    {
        av_log(nullptr, AV_LOG_INFO, "sdl create thread occur error\n");
        return -1;
    }
    SDL_Delay(10);

    int count = 0;
    while (++count < 50)
    {
        SDL_LockMutex(sdl_lock); // 对互斥锁加锁
        number = count;
        SDL_Delay(10);
        SDL_CondSignal(sdl_signal); // 发出相应信号
        SDL_UnlockMutex(sdl_lock);  // 对互斥锁解锁
        SDL_Delay(10);
    }
    int finish_status;
    SDL_WaitThread(sdl_thread1, &finish_status);
    av_log(NULL, AV_LOG_INFO, "sdl_thread1 finish_status=%d\n", finish_status);
    SDL_WaitThread(sdl_thread2, &finish_status);
    av_log(NULL, AV_LOG_INFO, "sdl_thread2 finish_status=%d\n", finish_status);
    SDL_DestroyCond(sdl_signal); // 销毁信号量
    SDL_DestroyMutex(sdl_lock);  // 销毁互斥锁

    return 0;
}