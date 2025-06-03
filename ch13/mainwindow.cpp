#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    FunctionTransfer::init(QThread::currentThreadId());
    // 创建一个垂直布局
    QVBoxLayout* vBox = new QVBoxLayout(this->centralWidget());
    vBox->setAlignment(Qt::AlignTop);  // 顶部对齐

    // 创建按钮空间
    QPushButton *btn_choose = new QPushButton();
    btn_choose->setText("打开视频文件");
    vBox->addWidget(btn_choose);  // 给布局添加控件

    // 创建标签
    QLabel* label = new QLabel();
    label->setText("这是一个显示文件路径的标签");
    label->setMaximumHeight(50); // 设置最大高度
    vBox->addWidget(label); // 给布局添加标签

    // 注册打开视频按钮的控件的单击事件，输入依次是按钮，事件，回调方法
    connect(btn_choose, &QPushButton::clicked, [=](){
        //对话框的输入参数依次是 上级窗口，对话框标题 默认目录 文件夹过滤器
        QString path = QFileDialog::getOpenFileName(this, "打开视频", "../file", "Video files(*.mp4 *.m4p *.mov)");
        sprintf(m_video_path, "%s", path.toStdString().c_str());
        qInfo() << "文件路径：" << m_video_path << '\n';
        char strBuffer[256] = {0};
        sprintf(strBuffer, "视频文件路径是：%s", m_video_path);
        label->setText(strBuffer);  // 设置标签文本
    });

    // 创建视频视图
    video_view = new VideoView();
    video_view->setMinimumWidth(480);  // 设置最小宽度
    video_view->setMinimumHeight(270);  // 设置最小高度
    vBox->addWidget(video_view);  // 给布局添加控件

    // 创建播放/停止按钮
    btn_play = new QPushButton();
    btn_play->setText("QImage开始播放");
    vBox->addWidget(btn_play);
    connect(btn_play, &QPushButton::clicked, [=](){
        playVideo(0);
    });

    // 创建暂停/恢复按钮
    btn_pause = new QPushButton();
    btn_pause->setText("暂停播放");
    vBox->addWidget(btn_pause);
    connect(btn_pause, &QPushButton::clicked, [=](){
        pauseVideo();
    });

    // 创建OpenGL播放按钮
    btn_play2 = new QPushButton();
    btn_play2->setText("OpenGL开始播放");
    vBox->addWidget(btn_play2);
    connect(btn_play2, &QPushButton::clicked, [=](){
        playVideo(1);
    });

    // 创建OpenGL视图
    opengl_view = new OpenglView(); // 创建OpenGL视图
    QSizePolicy policy;
    policy.setHorizontalPolicy(QSizePolicy::Expanding); // 水平方向允许伸展
    policy.setVerticalPolicy(QSizePolicy::Expanding); // 垂直方向允许伸展
    opengl_view->setSizePolicy(policy); // 设置控件的大小策略
    opengl_view->setMinimumWidth(480); // 设置最小宽度
    opengl_view->setMinimumHeight(270); // 设置最小高度
    vBox->addWidget(opengl_view); // 给布局添加OpenGL视图

}

MainWindow::~MainWindow()
{
    if(video_decoder != nullptr)
    {
        video_decoder->stop(); // 停止播放视频
    }

    delete video_view;
    // delete opengl_view;
    delete ui;
}

// 开始播放 / 停止播放
void MainWindow::playVideo(int play_type)
{
    if(strlen(m_video_path) <= 0)
    {
        QMessageBox::critical(this, "出错啦", "请先选择视频文件");
        return;
    }
    is_stop = !is_stop;
    if(is_stop)  // 停止播放
    {
        video_decoder->stop();  // 停止视频解码
        btn_pause->setEnabled(false);  // 禁用按钮
        is_pause = false;
    }else  // 开始播放
    {
        m_play_type = play_type;
        // 创建解码器
        video_decoder = new VideoDecoder(m_play_type, m_video_path, this);
        video_decoder->start(); // 开始解码
        btn_pause->setEnabled(true);
        btn_pause->setText(is_pause?"恢复播放":"暂停播放");
    }
    if(m_play_type == 0)
    {
        btn_play->setText(is_stop?"QImage开始播放":"QImage暂停播放");
    }else
    {
        btn_play2->setText(is_stop?"Opengl开始播放":"Opengl暂停播放");
    }
}

// 恢复播放/暂停播放
void MainWindow::pauseVideo()
{
    if(is_stop==true)
    {
        QMessageBox::critical(this, "出错啦", "请先播放视频文件");
        return;
    }
    is_pause = !is_pause;
    btn_pause->setText(is_pause?"恢复播放":"暂停播放");
    if(is_pause)
    {
        video_decoder->pause(); // 暂停视频播放
    }else
    {
        video_decoder->resum(); // 恢复视频播放
    }
}


// 显示视频画面，此函数不宜做耗时操作，否则会影响播放的流畅性
void MainWindow::onDisplayVideo(VideoFramePtr videoFrame)
{
    qInfo() << "onDisplayVideo frame->width=" << videoFrame->width() << ", frame->height=" << videoFrame->height() << '\n';
    if(m_play_type == 0)
    {
        video_view->onFrameDecoded(videoFrame); // 视频视图暂时画面
    }else
    {
        opengl_view->onFrameDecoded(videoFrame);  // OpenGL视图展示画面
    }
}

// 停止播放，通知界面修改按钮状态
void MainWindow::onStopPlay()
{
    is_stop = true;
    btn_play->setText("QImage开始播放");
    btn_play2->setText("OpenGL开始播放");
    is_pause = false;
    btn_pause->setText("暂停播放");
    btn_pause->setEnabled(false);
}
