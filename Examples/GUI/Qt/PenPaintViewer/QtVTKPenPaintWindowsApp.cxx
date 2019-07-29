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

#include <deque>

static const int DRAW_HZ = 45;

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

  vtkPenCallback()
    : FirstPointPlaced(false)
  {
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

  //std::deque<QPointF> Points;
  QPointF LastPoint;
  bool FirstPointPlaced;

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
      this->FirstPointPlaced = false;
      break;
    case TabletMoveEvent:
      if (this->IsDrawing)
      {
        //this->Picker->Pick(interactor->GetEventPosition()[0],
        //  interactor->GetEventPosition()[1],
        //  0.0, renderer);
        //QPointF currentPickPoint;
        //currentPickPoint.setX(this->Picker->GetPickPosition()[0]);
        //currentPickPoint.setY(this->Picker->GetPickPosition()[1]);
        ////this->Points.push_back(currentPickPoint);
        //if (!this->FirstPointPlaced)
        //{
        //  return;
        //}

        static double lastDraw = vtkTimerLog::GetUniversalTime();
        if (vtkTimerLog::GetUniversalTime() - lastDraw > 1.0 / DRAW_HZ)
        {
          this->Picker->Pick(interactor->GetEventPosition()[0],
            interactor->GetEventPosition()[1],
            0.0, renderer);
          QPointF currentPickPoint;
          currentPickPoint.setX(this->Picker->GetPickPosition()[0]);
          currentPickPoint.setY(this->Picker->GetPickPosition()[1]);

          if (currentPickPoint.x() <= 0 || currentPickPoint.y() <= 0)
            {
            this->FirstPointPlaced = false;
            return;
            }

          //this->Points.push_back(currentPickPoint);
          if (!this->FirstPointPlaced)
          {
            this->FirstPointPlaced = true;
            this->LastPoint = currentPickPoint;
            return;
          }

          lastDraw = vtkTimerLog::GetUniversalTime();
          double maxSize = 25.0;

          double color[4];
          this->Drawer->GetDrawColor(color);
          if (interactor->GetTabletPointer() == vtkRenderWindowInteractor::TabletPointerType::Eraser)
          {
            this->Drawer->SetDrawColor(0, 0, 0);
            /*this->Drawer->FillBox(this->Picker->GetPickPosition()[0] - maxSize * interactor->GetScale(), this->Picker->GetPickPosition()[0] + maxSize * interactor->GetScale(),
                                  this->Picker->GetPickPosition()[1] - maxSize * interactor->GetScale(), this->Picker->GetPickPosition()[1] + maxSize * interactor->GetScale());*/
          }

          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::LeftButton)
          {
            //this->Drawer->DrawCircle(
            //  this->Picker->GetPickPosition()[0],
            //  this->Picker->GetPickPosition()[1],
            //  maxSize * interactor->GetScale());
            //for (int i = 0; i < this->Points.size() - 1; ++i)
            //{
            //  QPointF point1 = this->Points[i];
            //  QPointF point2 = this->Points[i+1];
            //  this->Drawer->FillTube(point1.x(), point1.y(), point2.x(), point2.y(), maxSize * interactor->GetScale());
            //}
            //for (int i = 0; i < this->Points.size(); ++i)
            //{
            //  this->Drawer->DrawCircle(
            //    this->Points[i].x(),
            //    this->Points[i].y(),
            //    maxSize * interactor->GetScale());
            //}
            QPointF point1 = currentPickPoint;
            QPointF point2 = this->LastPoint;
            this->Drawer->FillTube(point1.x(), point1.y(), point2.x(), point2.y(), maxSize * interactor->GetScale());
          }
          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::RightButton)
          {
            this->Drawer->FillBox(this->Picker->GetPickPosition()[0] - maxSize * interactor->GetScale(), this->Picker->GetPickPosition()[0] + maxSize * interactor->GetScale(),
                                  this->Picker->GetPickPosition()[1] - maxSize * interactor->GetScale(), this->Picker->GetPickPosition()[1] + maxSize * interactor->GetScale());
          }
          this->Drawer->SetDrawColor(color);
          this->ImageViewer->Render();
          //this->Points.clear();
          this->LastPoint = currentPickPoint;
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
    qCritical() << "Microsoft Surface Pen: Single click";
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
  widget.setRenderWindow(renWin);

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

  QShortcut shortcut(&widget);
  shortcut.setKey(QKeySequence(Qt::Key_F20 + Qt::META));
  QObject::connect(&shortcut, &QShortcut::activated, &vtkPenCallback::MicrosoftSurfaceSingleClickPen);

  widget.show();
  imageViewer->Render();
  app.exec();
  return 0;
}
