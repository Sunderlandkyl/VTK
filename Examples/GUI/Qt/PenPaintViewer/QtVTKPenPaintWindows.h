#ifndef QtVTKPenPaintWindows_H
#define QtVTKPenPaintWindows_H

#include "vtkSmartPointer.h"
#include "vtkResliceImageViewer.h"
#include "vtkImagePlaneWidget.h"
#include "vtkDistanceWidget.h"
#include "vtkResliceImageViewerMeasurements.h"
#include <QMainWindow>

// Forward Qt class declarations
class Ui_QtVTKPenPaintWindows;

class QtVTKPenPaintWindows : public QMainWindow
{
  Q_OBJECT
public:

  // Constructor/Destructor
  QtVTKPenPaintWindows(int argc, char* argv[]);
  ~QtVTKPenPaintWindows() override {}

private:

  // Designer form
  Ui_QtVTKPenPaintWindows* ui;
};

#endif // QtVTKPenPaintWindows_H
