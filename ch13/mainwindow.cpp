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


}

MainWindow::~MainWindow()
{
    delete ui;
}
