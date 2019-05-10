#include "ui_QtVTKTouchscreenRenderWindows.h"
#include "QtVTKTouchscreenRenderWindows.h"

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCollectionIterator.h>
#include <vtkCubeSource.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleMultiTouchCamera.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

class vtkInteractorStyleMultiTouchCameraExample : public vtkInteractorStyleMultiTouchCamera
{
public:
  static vtkInteractorStyleMultiTouchCameraExample* New();
  vtkTypeMacro(vtkInteractorStyleMultiTouchCameraExample, vtkInteractorStyleMultiTouchCamera);

  void OnLongTap()
  {
    if (!this->CurrentRenderer)
    {
      return;
    }

    vtkCamera* camera = this->CurrentRenderer->GetActiveCamera();
    if (!camera)
    {
      return;
    }

    camera->SetParallelProjection(!camera->GetParallelProjection());
    this->CurrentRenderer->Render();
  }

  void OnTap()
  {
    if (!this->CurrentRenderer)
    {
      return;
    }

    this->CurrentRenderer->SetBackground((double)rand() / RAND_MAX,
                                         (double)rand() / RAND_MAX,
                                         (double)rand() / RAND_MAX);
  }

  void OnSwipe()
  {
    if (!this->CurrentRenderer)
    {
      return;
    }

    double hsv[3] = { this->Interactor->GetRotation() / 360.0, 1.0, 1.0 };
    double rgb[3];
    vtkMath::HSVToRGB(hsv, rgb);

    vtkActor* actor = nullptr;
    //vtkSmartPointer<vtkActorCollection> actors = vtkSmartPointer<vtkActorCollection>::Take(
    //  this->CurrentRenderer->GetActors());
    vtkActorCollection* actors = this->CurrentRenderer->GetActors();
    vtkCollectionSimpleIterator it;
    for (actors->InitTraversal(it);
      (actor = actors->GetNextActor(it)); )
    {
      actor->GetProperty()->SetColor(rgb);
    }
  }
};
vtkStandardNewMacro(vtkInteractorStyleMultiTouchCameraExample);

//----------------------------------------------------------------------------
QtVTKTouchscreenRenderWindows::QtVTKTouchscreenRenderWindows(int vtkNotUsed(argc), char* argv[])
{
  this->ui = new Ui_QtVTKTouchscreenRenderWindows;
  this->ui->setupUi(this);

  vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  this->ui->view->SetRenderWindow(renderWindow);

  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
  this->ui->view->GetRenderWindow()->AddRenderer(renderer);

  vtkRenderWindowInteractor* interactor = this->ui->view->GetInteractor();
  vtkSmartPointer<vtkInteractorStyleMultiTouchCameraExample> interactorStyle = vtkSmartPointer<vtkInteractorStyleMultiTouchCameraExample>::New();
  interactor->SetInteractorStyle(interactorStyle);
  renderWindow->SetInteractor(interactor);

  // Create a cube.
  vtkSmartPointer<vtkCubeSource> cubeSource =
    vtkSmartPointer<vtkCubeSource>::New();
  cubeSource->SetXLength(0.5);
  cubeSource->SetYLength(0.5);
  cubeSource->SetZLength(0.5);

  // Create a mapper and actor.
  vtkSmartPointer<vtkPolyDataMapper> mapper =
    vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputConnection(cubeSource->GetOutputPort());

  vtkSmartPointer<vtkActor> actor =
    vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  renderer->AddActor(actor);

  renderer->SetBackground(0.1, 0.2, 0.4);
};
