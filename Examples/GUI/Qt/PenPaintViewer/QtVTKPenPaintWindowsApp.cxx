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
#include <sstream>

static const int DRAW_HZ = 45;
enum PenBrushType
{
  Dot,
  Circle,
  Square,
  Line,
  LastBrushType
};

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

class vtkPenCallback;
vtkPenCallback* PenCallbackInstance;

//----------------------------------------------------------------------------
class vtkPenCallback : public vtkCallbackCommand
{
public:
  static vtkPenCallback* New()
  {
    return new vtkPenCallback;
  }
  vtkPenCallback()
    : Drawer(nullptr)
    , ImageViewer(nullptr)
    , Picker(nullptr)
    , BrushType(Circle)
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

  void Execute(vtkObject* vtkNotUsed(caller), unsigned long event, void* vtkNotUsed(callData)) override
  {
    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();
    vtkRenderer* renderer = this->ImageViewer->GetRenderer();
    switch (event)
    {
    case TabletPressEvent:
      this->IsDrawing = true;
      this->LastPoint = QPoint(-1, -1);
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
          this->Picker->Pick(interactor->GetEventPosition()[0],
            interactor->GetEventPosition()[1],
            0.0, renderer);
          QPointF currentPickPoint;
          currentPickPoint.setX(this->Picker->GetPickPosition()[0]);
          currentPickPoint.setY(this->Picker->GetPickPosition()[1]);

          lastDraw = vtkTimerLog::GetUniversalTime();
          double maxSize = 25.0;

          double color[4];
          this->Drawer->GetDrawColor(color);
          if (interactor->GetTabletPointer() == vtkRenderWindowInteractor::TabletPointerType::Eraser)
          {
            this->Drawer->SetDrawColor(0, 0, 0);
          }

          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::LeftButton)
          {
            switch (this->BrushType)
            {
            case Dot:
              this->Drawer->DrawPoint(
                this->Picker->GetPickPosition()[0],
                this->Picker->GetPickPosition()[1]);
              break;
            case Circle:
              this->Drawer->DrawCircle(
                this->Picker->GetPickPosition()[0],
                this->Picker->GetPickPosition()[1],
                maxSize * interactor->GetTabletPressure());
              break;
            case Line:
              if (this->LastPoint.x() >= 0)
              {
                this->Drawer->FillTube(currentPickPoint.x(), currentPickPoint.y(),
                  this->LastPoint.x(), this->LastPoint.y(), maxSize * interactor->GetTabletPressure());
              }
              break;
            case Square:
              this->Drawer->FillBox(this->Picker->GetPickPosition()[0] - maxSize * interactor->GetTabletPressure(), this->Picker->GetPickPosition()[0] + maxSize * interactor->GetTabletPressure(),
                this->Picker->GetPickPosition()[1] - maxSize * interactor->GetTabletPressure(), this->Picker->GetPickPosition()[1] + maxSize * interactor->GetTabletPressure());
              break;
            default:
              break;
            }
          }
          if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::RightButton)
          {

          }
          this->Drawer->SetDrawColor(color);
          this->ImageViewer->Render();
          this->LastPoint = currentPickPoint;
        }
      }
      break;
    case KeyPressEvent:
      if (strcmp(interactor->GetKeySym(), "period") == 0)
      {
        this->NextBrush();
      }
      else if (strcmp(interactor->GetKeySym(), "comma") == 0)
      {
        this->PreviousBrush();
      }
      else if (strcmp(interactor->GetKeySym(), "space") == 0)
      {
        this->ChangeColor();
      }
      else if (strcmp(interactor->GetKeySym(), "F20") == 0)
      {
        this->NextBrush();
      }
      else if (strcmp(interactor->GetKeySym(), "Escape") == 0)
      {
        this->EraseAll();
      }
      break;
    default:
      break;
    }
  }

  void NextBrush()
  {
    this->BrushType = (this->BrushType + 1) % LastBrushType;
  }

  void PreviousBrush()
  {
    this->BrushType--;
    if (this->BrushType < 0)
    {
      this->BrushType = LastBrushType - 1;
    }
  }

  void ChangeColor()
  {
    this->Drawer->SetDrawColor(
      255.0 * (double)rand() / RAND_MAX,
      255.0 * (double)rand() / RAND_MAX,
      255.0 * (double)rand() / RAND_MAX);
  }

  void EraseAll()
  {
    double color[4];
    this->Drawer->GetDrawColor(color);
    this->Drawer->SetDrawColor(0, 0, 0);
    vtkImageData* image = this->Drawer->GetOutput();
    int* extent = image->GetExtent();
    this->Drawer->FillBox(extent[0], extent[1], extent[2], extent[3]);
    this->Drawer->SetDrawColor(color);
    this->ImageViewer->Render();
  }

protected:
  vtkImageCanvasSource2D* Drawer;
  vtkImageViewer2* ImageViewer;
  vtkPropPicker* Picker;
  QPointF LastPoint;
  bool IsDrawing;
  int BrushType;
};

static void SurfacePenMagicButtonSingleClick()
{
  PenCallbackInstance->ChangeColor();
}

static void SurfacePenMagicButtonDoubleClick()
{
  PenCallbackInstance->NextBrush();
}

static void SurfacePenMagicButtonLongClick()
{
  PenCallbackInstance->EraseAll();
}

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
  widget.resize(imageSize*2, imageSize*2);
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
  PenCallbackInstance = callback;

  QShortcut singleClickShortcut(&widget);
  singleClickShortcut.setKey(QKeySequence(Qt::Key_F20 + Qt::META));
  QObject::connect(&singleClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonSingleClick);

  QShortcut doubleClickShortcut(&widget);
  doubleClickShortcut.setKey(QKeySequence(Qt::Key_F19 + Qt::META));
  QObject::connect(&doubleClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonDoubleClick);

  QShortcut longClickShortcut(&widget);
  longClickShortcut.setKey(QKeySequence(Qt::Key_F18 + Qt::META));
  QObject::connect(&longClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonLongClick);

  widget.show();
  imageViewer->Render();
  app.exec();
  return 0;
}
