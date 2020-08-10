#include <QApplication>
#include <QSurfaceFormat>

#include "QVTKOpenGLWidget.h"
#include "QtVTKTouchscreenRenderWindows.h"

int main(int argc, char** argv)
{
  //SetProcessDPIAware();

  // Needed to ensure appropriate OpenGL context is created for VTK rendering.
  QSurfaceFormat format = QVTKOpenGLWidget::defaultFormat();
#if _WINDOWS
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
#endif
  QSurfaceFormat::setDefaultFormat(format);

  //QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  //QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  // QT Stuff
  QApplication app(argc, argv);
  app.setAttribute(Qt::AA_CompressHighFrequencyEvents, true);

  QtVTKTouchscreenRenderWindows myQtVTKTouchscreenRenderWindows(argc, argv);
  myQtVTKTouchscreenRenderWindows.show();

  return app.exec();
}
