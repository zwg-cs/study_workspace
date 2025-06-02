#include "FunctionTransfer.h"

#include <QThread>
#include <QDebug>

Qt::HANDLE FunctionTransfer::gMainThreadId = nullptr;
FunctionTransfer *FunctionTransfer::main_thread_forward = nullptr;

Q_DECLARE_METATYPE(std::function<void()>)

// 构造函数
FunctionTransfer::FunctionTransfer(QObject *parent)
    : QObject{parent}
{
    // 因为std::function<void()> 是自定义的类型，所以跨线程传递先要注册一下
    qRegisterMetaType<std::function<void()>>();

    connect(this, SIGNAL(coming(std::function<void()>)), this, SLOT(slotExec(std::function<void()>)), Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(comming_noBlock(std::function<void()>)), this, SLOT(slotExec(std::function<void>())), Qt::QueuedConnection);
}

// 析构函数
FunctionTransfer::~FunctionTransfer()
{

}

//
void FunctionTransfer::init(Qt::HANDLE id)
{
    gMainThreadId = id;
    FunctionTransfer::main_thread_forward = new FunctionTransfer();
}

bool FunctionTransfer::isMainThread()
{
    if(gMainThreadId == nullptr)
    {
        qDebug() << __FILE__ << __LINE__ << __FUNCTION__<< "the main thread id is set!";
        return false;
    }

    if(QThread::currentThread() == gMainThreadId)
    {
        return true;
    }else
    {
        return false;
    }
}

void FunctionTransfer::runInMainThread(std::function<void()> f, bool isBlock)
{
    if(FunctionTransfer::isMainThread())
    {
        f();
    }
    else
    {
        if(isBlock)
        {
            Q_EMIT FunctionTransfer::main_thread_forward->comming(f);
        }else
        {
            Q_EMIT FunctionTransfer::main_thread_forward->comming_noBlock(f);
        }
    }
}

void FunctionTransfer::slotExec(std::function<void()> f)
{
    f();
}

