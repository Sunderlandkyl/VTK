
// Qt includes
#include <QApplication>
#include <QShortcut>
#include <QSurfaceFormat>

// VTK includes
#include "QVTKOpenGLNativeWidget.h"
#include "QVTKOpenGLWidget.h"
#include "QtVTKPenPaintWindows.h"
#include "vtkCallbackCommand.h"
#include "vtkDataArray.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkImageCanvasSource2D.h"
#include "vtkImageData.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkImageViewer.h"
#include "vtkInteractorStyleImage.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTimerLog.h"

static const int MAX_DRAW_HZ = 60;
static const double MAX_BRUSH_SIZE = 25.0;

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

//----------------------------------------------------------------------------
class vtkPenCallback : public vtkCallbackCommand
{
public:
  static vtkPenCallback* New() { return new vtkPenCallback; }
  vtkPenCallback()
    : Drawer(nullptr)
    , ImageViewer(nullptr)
    , BrushType(Circle)
    , LastDrawTime(vtkTimerLog::GetUniversalTime())

  {
  }

  void SetDrawer(vtkImageCanvasSource2D* drawer) { this->Drawer = drawer; }

  void SetImageViewer(vtkImageViewer2* imageViewer) { this->ImageViewer = imageViewer; }

  void Execute(
    vtkObject* vtkNotUsed(caller), unsigned long event, void* vtkNotUsed(callData)) override
  {
    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();
    switch (event)
    {
      case TabletPressEvent:
        if (interactor->GetTabletButtons() &
          vtkRenderWindowInteractor::TabletButtonType::LeftButton)
        {
          this->LastPoint = QPointF(-1.0, -1.0);
          this->IsDrawing = true;
        }
        break;
      case TabletReleaseEvent:
        this->IsDrawing = false;
        break;
      case TabletMoveEvent:
        this->OnTabletMove();
        break;
      case KeyPressEvent:
        this->OnKeyPress();
        break;
      default:
        break;
    }
  }

  void OnTabletMove()
  {
    // Tablet event frequency is very high. We can't update the image at that speed, so limit
    // updates to the specified frequency.
    if (vtkTimerLog::GetUniversalTime() - this->LastDrawTime < 1.0 / MAX_DRAW_HZ)
    {
      return;
    }
    this->LastDrawTime = vtkTimerLog::GetUniversalTime();

    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();
    vtkRenderer* renderer = this->ImageViewer->GetRenderer();

    double displayCoordinates[3] = { static_cast<double>(interactor->GetEventPosition()[0]),
      static_cast<double>(interactor->GetEventPosition()[1]), 0.0 };
    renderer->SetDisplayPoint(displayCoordinates);
    renderer->DisplayToWorld();
    double worldCoordinates[3] = { 0.0, 0.0, 0.0 };
    renderer->GetWorldPoint(worldCoordinates);
    int imageCoordinates[3] = { static_cast<int>(worldCoordinates[0]),
      static_cast<int>(worldCoordinates[1]), static_cast<int>(worldCoordinates[2]) };

    if (interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::RightButton)
    {
      this->EyeDropper(imageCoordinates);
    }
    else if (this->IsDrawing &&
      interactor->GetTabletButtons() & vtkRenderWindowInteractor::TabletButtonType::LeftButton)
    {
      if (interactor->GetTabletPointer() == vtkRenderWindowInteractor::TabletPointerType::Eraser)
      {
        this->Erase(imageCoordinates);
      }
      else
      {
        this->Draw(imageCoordinates);
      }
      this->ImageViewer->Render();
      this->LastPoint = QPointF(worldCoordinates[0], worldCoordinates[1]);
    }
  }

  void OnKeyPress()
  {
    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();
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
    else if (strcmp(interactor->GetKeySym(), "Escape") == 0)
    {
      this->EraseAll();
    }
  }

  void Draw(int imageCoordinates[3])
  {
    vtkRenderWindowInteractor* interactor = this->ImageViewer->GetRenderWindow()->GetInteractor();

    double pressure = interactor->GetTabletPressure();
    double radius = MAX_BRUSH_SIZE * pressure;
    switch (this->BrushType)
    {
      case Dot:
        this->Drawer->DrawPoint(imageCoordinates[0], imageCoordinates[1]);
        break;
      case Circle:
        this->Drawer->DrawCircle(imageCoordinates[0], imageCoordinates[1], radius);
        break;
      case Line:
        if (this->LastPoint.x() >= 0)
        {
          this->Drawer->FillTube(imageCoordinates[0], imageCoordinates[1], this->LastPoint.x(),
            this->LastPoint.y(), radius);
        }
        break;
      case Square:
        this->Drawer->FillBox(imageCoordinates[0] - radius, imageCoordinates[0] + radius,
          imageCoordinates[1] - radius, imageCoordinates[1] + radius);
        break;
      default:
        break;
    }
  }

  void Erase(int imageCoordinates[3])
  {
    double color[4];
    this->Drawer->GetDrawColor(color);
    this->Drawer->SetDrawColor(0, 0, 0);
    this->Draw(imageCoordinates);
    this->Drawer->SetDrawColor(color);
  }

  void EyeDropper(int imageCoordinates[3])
  {
    vtkImageData* imageData = this->Drawer->GetOutput();
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    imageData->GetExtent(extent);
    if (imageCoordinates[0] >= extent[0] && imageCoordinates[0] <= extent[1] &&
      imageCoordinates[1] >= extent[2] && imageCoordinates[1] <= extent[3])
    {
      // Set the color from the current voxel
      double color[4] = { 0.0, 0.0, 0.0, 1.0 };
      color[0] =
        imageData->GetScalarComponentAsDouble(imageCoordinates[0], imageCoordinates[1], 0, 0);
      color[1] =
        imageData->GetScalarComponentAsDouble(imageCoordinates[0], imageCoordinates[1], 0, 1);
      color[2] =
        imageData->GetScalarComponentAsDouble(imageCoordinates[0], imageCoordinates[1], 0, 2);
      this->Drawer->SetDrawColor(color);
    }
  }

  void NextBrush()
  {
    this->BrushType++;
    this->BrushType %= LastBrushType;
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
      vtkMath::Random(0.0, 255.0), vtkMath::Random(0.0, 255.0), vtkMath::Random(0.0, 255.0), 1.0);
  }

  void EraseAll()
  {
    vtkImageData* image = this->Drawer->GetOutput();
    image->GetPointData()->GetScalars()->Fill(0.0);
    this->Drawer->Modified();
    this->ImageViewer->Render();
  }

  static vtkSmartPointer<vtkPenCallback> Instance;

protected:
  vtkImageCanvasSource2D* Drawer;
  vtkImageViewer2* ImageViewer;
  QPointF LastPoint;
  bool IsDrawing;
  int BrushType;
  double LastDrawTime;
};

vtkSmartPointer<vtkPenCallback> vtkPenCallback::Instance = vtkSmartPointer<vtkPenCallback>::New();

static void SurfacePenMagicButtonSingleClick()
{
  vtkPenCallback::Instance->NextBrush();
}

static void SurfacePenMagicButtonDoubleClick()
{
  vtkPenCallback::Instance->PreviousBrush();
}

static void SurfacePenMagicButtonLongClick()
{
  vtkPenCallback::Instance->ChangeColor();
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
  widget.resize(imageSize * 2, imageSize * 2);
  vtkNew<vtkGenericOpenGLRenderWindow> renWin;
  widget.setRenderWindow(renWin);

  vtkSmartPointer<vtkImageCanvasSource2D> drawing = vtkSmartPointer<vtkImageCanvasSource2D>::New();
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

  vtkSmartPointer<vtkPenCallback> callback = vtkPenCallback::Instance;
  callback->SetDrawer(drawing);
  callback->SetImageViewer(imageViewer);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletPressEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletReleaseEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::TabletMoveEvent, callback);
  renWin->GetInteractor()->AddObserver(vtkCommand::KeyPressEvent, callback);

  QShortcut singleClickShortcut(&widget);
  singleClickShortcut.setKey(QKeySequence(Qt::Key_F20 + Qt::META));
  QObject::connect(
    &singleClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonSingleClick);

  QShortcut doubleClickShortcut(&widget);
  doubleClickShortcut.setKey(QKeySequence(Qt::Key_F19 + Qt::META));
  QObject::connect(
    &doubleClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonDoubleClick);

  QShortcut longClickShortcut(&widget);
  longClickShortcut.setKey(QKeySequence(Qt::Key_F18 + Qt::META));
  QObject::connect(&longClickShortcut, &QShortcut::activated, &::SurfacePenMagicButtonLongClick);

  widget.show();
  imageViewer->Render();
  app.exec();
  return 0;
}
