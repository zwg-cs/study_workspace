#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    FunctionTransfer::init(QThread::currentThreadId());
    // 创建一个垂直布局
    QVBoxLayout* vBox = new QVBoxLayout(this->centralWidget());
    vBox->setAlignment(Qt::AlignTop);
    // 选择文件按钮
    QPushButton*  btn_choose = new QPushButton();
    btn_choose->setText("打开音视频文件");
    vBox->addWidget(btn_choose);
    // 显示文件路径视图
    QLabel *label = new QLabel();
    label->setText("用于显示音视频文件路径的视图");
    vBox->addWidget(label);
    // 注册按钮的单击事件：按钮，事件类型，回调方法
    connect(btn_choose, &QPushButton::clicked, [=](){
        QString path = QFileDialog::getOpenFileName(this, "打开音视频", "../file",
                                                    "Video files(*.mp4 *.m4v *.mov *.3gp *.avc *.hevc *.ts *.flv *.asf *.wmv *.avi *.mkv *.mpg *.rm *.rmvb *.vob *.webm);;"
                                                    "Audio files(*.mp3 *.aac *.m4a *.wav *.ra *.ogg *.amr *.wma *.opus)");
        if(!path.isEmpty())
        {
            sprintf(m_video_path, "%s", path.toStdString().c_str());
            qInfo() << "文件路径：" << m_video_path << '\n';
            label->setText(path);
            if(video_player != nullptr && !is_stop)
            {
                playVideo();
            }
        }
    });

    // 创建水平布局，放置播放和暂停按钮
    QHBoxLayout* hBox = new QHBoxLayout();
    btn_play = new QPushButton;
    btn_play->setText("播放音视频");
    // 为播放按钮注册单击事件
    connect(btn_play, &QPushButton::clicked, [=](){
        playVideo();  // 开始播放/停止播放
    });
    hBox->addWidget(btn_play);
    btn_pause = new QPushButton;
    btn_pause->setText("暂停播放");
    // 为暂停播放按钮注册单击事件
    connect(btn_pause, &QPushButton::clicked, [=](){
        pauseVideo();  // 暂停播放/恢复播放
    });
    hBox->addWidget(btn_pause);
    vBox->addLayout(hBox);

    // 创建Opengl视图
    opengl_view = new OpenglView();
    QSizePolicy policy;
    policy.setHorizontalPolicy(QSizePolicy::Expanding); // 水平方向允许伸展
    policy.setVerticalPolicy(QSizePolicy::Expanding); // 垂直方向允许伸展
    opengl_view->setMinimumWidth(480);  // 设置最小宽度
    opengl_view->setMinimumHeight(270);  // 设置最小高度
    vBox->addWidget(opengl_view);

    std::map<QString, QString> program_map;
    program_map["  在此选择直播网址"] = "";
    program_map["2018年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2018/04/17/20180417_162d3639356_r38_1200k.mp4";
    program_map["2019年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2019/04/15/20190415_16a1ef11c24_r38_1200k.mp4";
    program_map["2020年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2020/09/26/20200926_174c8f9e4b6_r38_1200k.mp4";
    program_map["2021年数字中国峰会迎宾曲"] = "http://flv4mp4.people.com.cn/videofile7/pvmsvideo/2021/3/20/FuJianWuZhou_d0cdcc84ccf1f8e561544d422e470a7f.mp4";
    program_map["2022年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2022/07/15/20220715_18201603713_r38_1200k.mp4";
    program_map["2023年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2023/04/25/20230425_187b71018de_r38_1200k.mp4";
    program_map["福建三明新闻综合频道"] = "https://stream.smntv.cn/smtv1/playlist.m3u8";
    program_map["福建三明公共频道"] = "https://stream.smntv.cn/smtv2/playlist.m3u8";
    program_map["江苏南京少儿频道"] = "https://live.nbs.cn/channels/njtv/sepd/m3u8:500k/live.m3u8";
    program_map["江苏南京娱乐频道"] = "https://live.nbs.cn/channels/njtv/ylpd/m3u8:500k/live.m3u8";
    program_map["江苏南京十八频道"] = "https://live.nbs.cn/channels/njtv/sbpd/m3u8:500k/live.m3u8";
    program_map["江苏南京信息频道"] = "https://live.nbs.cn/channels/njtv/xxpd/m3u8:500k/live.m3u8";
    program_map["广西南宁新闻综合频道"] = "https://hls.nntv.cn/nnlive/NNTV_NEWS_A.m3u8";
    program_map["广西南宁都市生活频道"] = "https://hls.nntv.cn/nnlive/NNTV_METRO_A.m3u8";
    program_map["广西南宁影视娱乐频道"] = "https://hls.nntv.cn/nnlive/NNTV_VOD_A.m3u8";
    program_map["广西南宁公共频道"] = "https://hls.nntv.cn/nnlive/NNTV_PUB_A.m3u8";
    program_map["广东河源综合频道"] = "https://tmpstream.hyrtv.cn/xwzh/sd/live.m3u8";
    program_map["广东河源公共频道"] = "https://tmpstream.hyrtv.cn/hygg/sd/live.m3u8";

    // 创建下拉框控件
    QComboBox* program_list = new QComboBox();
    std::map<QString, QString>::iterator it;
    for(it = program_map.begin(); it != program_map.end(); ++it)
    {
        program_list->addItem(it->first);
    }
    // 给布局添加下拉框控件
    vBox->addWidget(program_list);
    // 注册按钮控件的单击事件
    connect(program_list, QOverload<const QString &>::of(&QComboBox::currentTextChanged), this,
            [=](const QString &text){
        if(!text.isEmpty() && !program_map.at(text).isEmpty())
        {
            sprintf(m_video_path,  "%s", program_map.at(text).toStdString().c_str());
            qInfo() << "直播路径：" << m_video_path << '\n';
            char strBuffer[256] = {0};
            sprintf(strBuffer, "音视频直播路径是：%s", m_video_path);
            label->setText(strBuffer); // 设置标签文本
            if (video_player != NULL && !is_stop) {
                playVideo();
                _sleep(200);
            }
            // playVideo();
        }
    });
}

MainWindow::~MainWindow()
{
    if(video_player != nullptr)
    {
        video_player->stop();  // 停止播放
    }
    delete ui;
}


// 开始播放/停止播放
void MainWindow::playVideo()
{
    if(m_video_path != nullptr || strlen(m_video_path) <= 0)
    {
        QMessageBox::critical(this, "出错啦", "请先选择视频文件");
        return;
    }
    is_stop = !is_stop;
    btn_play->setText(is_stop?"开始播放":"停止播放");
    if(is_stop)
    {
        video_player->stop(); // 停止播放
        btn_pause->setEnabled(false);
        is_pause = false;
    }else
    {
        btn_pause->setEnabled(true);
        btn_pause->setText(is_pause? "恢复播放":"暂停播放");
        video_player = new VideoPlayer(m_video_path, this);
        const char* result = video_player->start(); // 开始播放
        if(result != nullptr)
        {
            QMessageBox::critical(this, "播放失败", result);
        }
    }
    qInfo() << "opengl_view->width=" << opengl_view->width() << ", opengl_view->height=" << opengl_view->height() << '\n';
}

// 恢复播放/暂停播放
void MainWindow::pauseVideo()
{
    if(is_stop == true)
    {
        QMessageBox::critical(this, "出错啦", "请先播放视频文件");
        return;
    }
    is_pause = !is_pause;
    if(is_pause)
    {
        video_player->pause(); // 暂停播放
    }else
    {
        video_player->resume(); // 恢复播放
    }
}

// 显示视频画面，此函数不宜做耗时操作，否则会影响播放的流畅性。
void MainWindow::onDisplayVideo(VideoFramePtr videoFrame)
{
    qInfo() << "onDisplayVideo frame->width=" << videoFrame->width() << ", video->height" << videoFrame->height() << '\n';
    opengl_view->onFrameDecode(videoFrame);  // opengl视图展示画面
}

// 停止播放，通知界面修改按钮状态
void MainWindow::onStopVideo()
{
    is_stop = true;
    btn_play->setText("开始播放");
    is_pause = false;
    btn_pause->setText("暂停播放");
    btn_pause->setEnabled(false);
}

