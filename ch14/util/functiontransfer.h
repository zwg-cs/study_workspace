#ifndef FUNCTIONTRANSFER_H
#define FUNCTIONTRANSFER_H

/*
这个 FunctionTransfer 类是一个 用于在线程间（特别是非主线程 → 主线程）转发函数调用的 Qt 工具类，
常用于 Qt 多线程开发中让某个函数在 主线程中安全执行，比如更新 UI 元素。
*/
#include <functional>

#include <QThread>

#include <QObject>

class FunctionTransfer : public QObject
{
    // Q_OBJECT 是 Qt 提供的宏，用来启用：信号（signals） 槽函数（slots） QObject::metaObject() 等反射能力 属性系统（如 Q_PROPERTY） 类型信息（运行时类型识别）
    Q_OBJECT
public:
    // 这是一个构造函数声明，带一个 默认参数 parent，用来指定 Qt 的父子对象关系。在 Qt 中，如果你传入 parent，Qt 会自动帮你管理内存：当父对象销毁时，子对象也会自动销毁。
    explicit FunctionTransfer(QObject* parent = 0);
    // 析构函数
    ~FunctionTransfer();

    // 初始化：传入主线程id，创建主线程中的fucntionTransfer实例
    static void init(Qt::HANDLE id);

    // 判断当前线程是否是主线程
    static bool isMainThread();

    // 将函数f调度到主线程执行，isBlock = true 表示调用线程阻塞到 f 执行完成
    static void runInMainThread(std::function<void()> f, bool isBlock = false);

private:
    // 保存主线程的id
    static Qt::HANDLE gMainThreadId;

    // 主线程中的FunctionTransfer 实例，用于发出信号
    static FunctionTransfer* main_thread_forward;

Q_SIGNALS:
    // 发射该信号以“阻塞方式”将函数对象传递到主线程  (BlockingQueuedConnection)
    void comming(std::function<void ()> f);
    // 发射该信号以“非阻塞方式”传递函数对象到主线程 (QueuedConnection)
    void comming_noBlock(std::function<void ()> f);

private Q_SLOTS:
    // 真正执行函数对象的槽函数，运行在主线程中
    void slotExec(std::function<void ()> f);
};

#endif // FUNCTIONTRANSFER_H
