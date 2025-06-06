#include "functiontransfer.h"

#include <QThread>
#include <QDebug>

// 静态成语变量定义
Qt::HANDLE FunctionTransfer::gMainThreadId = nullptr;  // 主线程句柄
FunctionTransfer *FunctionTransfer::main_thread_forward; // 主线程中的实例

// 允许 std::function<void ()> 作为信号/槽函数传递
Q_DECLARE_METATYPE(std::function<void()>)


FunctionTransfer::FunctionTransfer(QObject* parent) : QObject(parent)
{
    // 注册std::function<void()>类型，使其可以用于Qt信号槽传递
    qRegisterMetaType<std::function<void()>>();

    // 连接comming 信号到 slotExec 槽，阻塞调用（调用线程会等待主线程执行完毕）
    connect(this, SIGNAL(comming(std::function<void()>)), this, SLOT(slotExec(std::function<void()>)), Qt::BlockingQueuedConnection);

    // 连接comming_noBlock 信号到 slotExec 槽，异步调用（调用线程立即返回）
    connect(this, SIGNAL(comming_noBlock(std::function<void()>)), this, SLOT(slotExec(std::function<void()>)), Qt::QueuedConnection);
}

FunctionTransfer::~FunctionTransfer()
{}


// 初始化：设置主线程id，在主线程中创建FunctionTransfer实例
void FunctionTransfer::init(Qt::HANDLE id)
{
    gMainThreadId = id;

    // 实例化一个对象，其线程归属当前线程
    FunctionTransfer::main_thread_forward = new FunctionTransfer();
    qDebug() << "main_thread_forward:" << FunctionTransfer::main_thread_forward;

}


// 判断当前线程是否为主线程
bool FunctionTransfer::isMainThread()
{
    if(gMainThreadId == nullptr)
    {
        qDebug() << __FILE__<<__LINE__<<__FUNCTION__<<"the main thread id is not set!";
        return false;
    }

    if (QThread::currentThreadId() == gMainThreadId)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// 外部统一入口：：确保某个函数在主线程中执行
void FunctionTransfer::runInMainThread(std::function<void()> f, bool isBlock)
{
    if(FunctionTransfer::isMainThread())
    {
        // 如果本身就是主线程，直接执行
        f();
    }
    else
    {
        //如果在非主线程，根据是否阻塞执行来选择发送的信号
        if(isBlock)
        {
            // 阻塞模式，主线程执行 f() 前调用线程会等待
            Q_EMIT FunctionTransfer::main_thread_forward->comming(f);
        }
        else
        {
            // 非阻塞模型，立即返回, f() 稍后在主线程中异步执行
            Q_EMIT FunctionTransfer::main_thread_forward->comming_noBlock(f);
        }
    }
}

// 槽函数：在主线程中执行函数对象
void FunctionTransfer::slotExec(std::function<void()> f)
{
    f();  // 直接执行函数
}
