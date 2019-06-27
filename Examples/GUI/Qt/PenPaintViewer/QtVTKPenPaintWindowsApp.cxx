#include <QApplication>
#include <QSurfaceFormat>
#include <QShortcut>
#include <QDebug>

#include "QVTKOpenGLWidget.h"
#include "QtVTKPenPaintWindows.h"

#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkImageViewer.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkPNGReader.h"
#include "vtkTestUtilities.h"
#include "vtkImageBlend.h"
#include "vtkImageCanvasSource2D.h"
#include "vtkImageData.h"
#include "vtkCallbackCommand.h"
#include "QVTKOpenGLNativeWidget.h"
#include "vtkPropPicker.h"
#include "vtkObjectFactory.h"
#include "vtkInteractorStyleImage.h"
#include "vtkImageActor.h"
#include "vtkLookupTable.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkTimerLog.h"

static const int DRAW_HZ = 30;
static const int RENDER_HZ = 30;

//----------------------------------------------------------------------------
class vtkPenInteractorExample : public vtkInteractorStyleImage
{
public:
  static vtkPenInteractorExample* New();
  vtkTypeMacro(vtkPenInteractorExample, vtkInteractorStyleImage);

  void OnMouseMove() override {}
  void OnLeftButtonDown() override {}
  void OnRightButtonDown() override {}
};
vtkStandardNewMacro(vtkPenInteractorExample);

//----------------------------------------------------------------------------
class vtkPenCallback : public vtkCallbackCommand
{
public:
  static vtkPenCallback* New()
  {
    return new vtkPenCallback;
  }

  void SetDrawer(vtkImageCanvasSource2D* drawer)
  {
    this->Drawer = drawer;
  }

  void SetImageViewer(vtkImageViewer2* imageViewer)
  {
    this->ImageViewer = imageViewer;
  }

  void SetPicker(vtkPropPicker* picker)
  {
    this->Picker = picker;
  }

  void Execute(vtkObject* vtkNotUsed(caller), unsigned long event, void* vtkNotUsed(callData)) override
  {
    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();
    vtkRenderer* renderer = this->ImageViewer->GetRenderer();
    switch (event)
    {
    case TabletPressEvent:
      this->IsDrawing = true;
      break;
    case TabletReleaseEvent:
      this->IsDrawing = false;
      break;
    case TabletMoveEvent:
      if (this->IsDrawing)
      {
        static double lastDraw = vtkTimerLog::GetUniversalTime();
        if (vtkTimerLog::GetUniversalTime() - lastDraw > 1.0 / DRAW_HZ)
        {
          lastDraw = vtkTimerLog::GetUniversalTime();
          double maxSize = 25.0;
          this->Picker->Pick(interactor->GetEventPosition()[0],
            interactor->GetEventPosition()[1],
            0.0, renderer);

          double color[4];
          this->Drawer->GetDrawColor(color);
          if (interactor->GetTabletPointer() == vtkRenderWindowInteractor::TabletPointerType::Eraser)
          {
            this->Drawer->SetDrawColor(0, 0, 0);
          }

          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::LeftButton)
          {
            this->Drawer->DrawCircle(
              this->Picker->GetPickPosition()[0],
              this->Picker->GetPickPosition()[1],
              maxSize * interactor->GetScale());
          }
          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::RightButton)
          {
            this->Drawer->DrawPoint(
              this->Picker->GetPickPosition()[0],
              this->Picker->GetPickPosition()[1]);
          }
          this->Drawer->SetDrawColor(color);
        }
        static double lastRender = vtkTimerLog::GetUniversalTime();
        if (vtkTimerLog::GetUniversalTime() - lastRender > 1.0 / RENDER_HZ)
        {
          lastRender = vtkTimerLog::GetUniversalTime();
          this->ImageViewer->Render();
        }
      }
      break;
    case KeyPressEvent:
      this->Drawer->SetDrawColor(
        255.0 * (double)rand() / RAND_MAX,
        255.0 * (double)rand() / RAND_MAX,
        255.0 * (double)rand() / RAND_MAX);
      break;
    default:
      break;
    }

  }

  static void MicrosoftSurfaceSingleClickPen()
  {
    // Qt does not currently detect pen buttons unless they are clicked on the screen
  }

protected:
  vtkImageCanvasSource2D* Drawer;
  vtkImageViewer2* ImageViewer;
  vtkPropPicker* Picker;
  bool IsDrawing;
};

int main(int argc, char** argv)
{
  // Needed to ensure appropriate OpenGL context is created for VTK rendering.
  QSurfaceFormat format = QVTKOpenGLWidget::defaultFormat();
#if _WINDOWS
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
#endif
  QSurfaceFormat::setDefaultFormat(format);

  int imageSize = 512;
  QApplication app(argc, argv);
  QVTKOpenGLNativeWidget widget;

  widget.resize(imageSize, imageSize);
  vtkNew<vtkGenericOpenGLRenderWindow> renWin;
  widget.SetRenderWindow(renWin);

  vtkSmartPointer<vtkImageCanvasSource2D> drawing =
    vtkSmartPointer<vtkImageCanvasSource2D>::New();
  drawing->SetNumberOfScalarComponents(3);
  drawing->SetScalarTypeToUnsignedChar();
  drawing->SetExtent(0, imageSize - 1, 0, imageSize - 1, 0, 0);
  drawing->SetDrawColor(0, 0, 0);
  drawing->FillBox(0, imageSize - 1, 0, imageSize - 1);
  drawing->SetDrawColor(255.0, 0.0, 0.0);

  vtkSmartPointer<vtkPenInteractorExample> renderWindowInteractorStyle =
    vtkSmartPointer<vtkPenInteractorExample>::New();

  vtkNew<vtkImageViewer2> imageViewer;
  imageViewer->SetRenderWindow(renWin);
  imageViewer->SetInputConnection(drawing->GetOutputPort());
  imageViewer->SetupInteractor(renWin->GetInteractor());
  renWin->GetInteractor()->SetInteractorStyle(renderWindowInteractorStyle);
  renderWindowInteractorStyle->SetDefaultRenderer(imageViewer->GetRenderer());
  renderWindowInteractorStyle->SetCurrentRenderer(imageViewer->GetRenderer());

  imageViewer->SetSize(imageSize, imageSize);
  imageViewer->GetRenderer()->ResetCamera();
  imageViewer->GetRenderer()->SetBackground(1, 1, 1); // black

  vtkNew<vtkPropPicker> picker;
  picker->PickFromListOn();
  picker->AddPickList(imageViewer->GetImageActor());

  vtkNew<vtkPenCallback> callback;
  callback->SetDrawer(drawing);
  callback->SetImageViewer(imageViewer);
  callback->SetPicker(picker);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletPressEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletReleaseEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletMoveEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::KeyPressEvent, callback);

  //QShortcut shortcut(&widget);
  //shortcut.setKey(QKeySequence(Qt::Key_F20 + Qt::META));
  //QObject::connect(&shortcut, &QShortcut::activated, &vtkPenCallback::MicrosoftSurfaceSingleClickPen);

  widget.show();

  app.exec();
  return 0;
}
