#include <vtkSmartPointer.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkShaderProgram.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkLookupTable.h>
#include <vtkOpenGLShaderAlgorithm.h>
#include <vtkTextureObject.h>
#include <vtkOpenGLImageToTextureFilter.h>
#include <vtkOpenGLTextureToImageFilter.h>
#include <vtkTimerLog.h>

#include <string>
#include <fstream>
#include <streambuf>

#include <vtkNrrdReader.h>
#include <vtkMetaImageWriter.h>

int main(int, char*[])
{
  std::ifstream vertFile("E:/d/v/SlicerShaderApp/noise.vert");
  std::string vertShader((std::istreambuf_iterator<char>(vertFile)),
    std::istreambuf_iterator<char>());
  std::ifstream cFragFile("E:/d/v/SlicerShaderApp/check.frag");
  std::string cFragShader((std::istreambuf_iterator<char>(cFragFile)),
    std::istreambuf_iterator<char>());
  std::ifstream nFragFile("E:/d/v/SlicerShaderApp/noise.frag");
  std::string nFragShader((std::istreambuf_iterator<char>(nFragFile)),
    std::istreambuf_iterator<char>());

  std::ifstream gaussianVSFile("E:/d/v/SlicerVTK/Rendering/OpenGL2/glsl/vtkGaussianBlurPassVS.glsl");
  std::string gaussianVSSource((std::istreambuf_iterator<char>(gaussianVSFile)),
    std::istreambuf_iterator<char>());

  std::ifstream gaussianFSFile("E:/d/v/SlicerShaderApp/gauss.frag");
  std::string gaussianFSSource((std::istreambuf_iterator<char>(gaussianFSFile)),
    std::istreambuf_iterator<char>());

  vtkNew<vtkNrrdReader> reader;
  reader->SetFileName("C:/Users/kyles/Desktop/MRHead.nrrd");
  reader->Update();
  vtkNew<vtkImageData> inputImage;
  inputImage->DeepCopy(reader->GetOutput());

  vtkNew<vtkTimerLog> timer;

  timer->StartTimer();
  vtkSmartPointer<vtkOpenGLRenderWindow> renderWindow = vtkOpenGLRenderWindow::SafeDownCast(vtkSmartPointer<vtkRenderWindow>::New());
  renderWindow->SetOffScreenRendering(true);
  renderWindow->Initialize();

  timer->StopTimer();
  std::cout << "Establishing rendering context took: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  vtkNew<vtkOpenGLImageToTextureFilter> inputConvert;
  inputConvert->SetRenderWindow(renderWindow);
  inputConvert->SetInputDataObject(inputImage);

  vtkNew<vtkOpenGLShaderAlgorithm> checkerPatternGenerator;
  checkerPatternGenerator->SetVertexShaderCode(vertShader);
  checkerPatternGenerator->SetFragmentShaderCode(cFragShader);
  checkerPatternGenerator->SetOutputExtent(inputImage->GetExtent());
  checkerPatternGenerator->SetRenderWindow(renderWindow);
  checkerPatternGenerator->SetRenderWindow(inputConvert->GetRenderWindow());

  vtkNew<vtkOpenGLShaderAlgorithm> shaderAlgorithm;
  shaderAlgorithm->SetVertexShaderCode(vertShader);
  shaderAlgorithm->SetFragmentShaderCode(nFragShader);
  shaderAlgorithm->SetOutputExtent(inputImage->GetExtent());
  shaderAlgorithm->AddInputConnection(inputConvert->GetOutputPort());
  shaderAlgorithm->AddInputConnection(checkerPatternGenerator->GetOutputPort());

  vtkNew<vtkOpenGLShaderAlgorithm> gaussianAlgorithm;
  gaussianAlgorithm->SetVertexShaderCode(vertShader);
  gaussianAlgorithm->SetFragmentShaderCode(gaussianFSSource);
  gaussianAlgorithm->SetOutputExtent(inputImage->GetExtent());
  gaussianAlgorithm->SetOutputScalarTypeToShort();
  gaussianAlgorithm->AddInputConnection(inputConvert->GetOutputPort());

  vtkNew<vtkOpenGLTextureToImageFilter> outputConvert;
  outputConvert->SetInputConnection(gaussianAlgorithm->GetOutputPort());
  outputConvert->Update();

  vtkSmartPointer<vtkImageData> outputImage = outputConvert->GetOutput();

  timer->StopTimer();
  std::cout << "Shader algorithm completed: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  vtkNew<vtkMetaImageWriter> writer;
  writer->SetInputData(outputImage);
  writer->SetFileName("C:/Users/kyles/Desktop/ShaderTest.mhd");
  writer->SetFileName("C:/Users/kyles/Desktop/ShaderTest.raw");
  writer->Write();

  vtkNew<vtkMetaImageWriter> writer2;
  writer2->SetInputData(inputImage);
  writer2->SetFileName("C:/Users/kyles/Desktop/ShaderOriginal.mhd");
  writer2->SetFileName("C:/Users/kyles/Desktop/ShaderOriginal.raw");
  writer2->Write();

  // Visualize
  vtkSmartPointer<vtkImageViewer2> imageViewer =
    vtkSmartPointer<vtkImageViewer2>::New();
  imageViewer->SetInputData(outputImage);
  imageViewer->GetRenderWindow()->SetSize(500, 500);
  imageViewer->GetWindowLevel()->SetWindow(imageViewer->GetInput()->GetScalarRange()[1] - imageViewer->GetInput()->GetScalarRange()[0]);
  imageViewer->GetWindowLevel()->SetLevel((imageViewer->GetInput()->GetScalarRange()[1] - imageViewer->GetInput()->GetScalarRange()[0]) / 2.0);
  imageViewer->GetRenderer()->ResetCamera();
  imageViewer->SetSlice(50);

  // Set up an interactor that does not respond to mouse events
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetInteractorStyle(0);
  imageViewer->GetRenderWindow()->SetInteractor(renderWindowInteractor);
  imageViewer->Render();

  // Start the event loop
  renderWindowInteractor->Initialize();
  renderWindowInteractor->Start();

  return EXIT_SUCCESS;
}