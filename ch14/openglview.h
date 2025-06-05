#ifndef OPENGLVIEW_H
#define OPENGLVIEW_H

#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QFile>

#include "util/FunctionTransfer.h"
#include "util/VideoFrame.h"

class OpenglView : public QOpenGLWidget,protected QOpenGLFunctions
{
    Q_OBJECT
public:
    OpenglView();
};

#endif // OPENGLVIEW_H
