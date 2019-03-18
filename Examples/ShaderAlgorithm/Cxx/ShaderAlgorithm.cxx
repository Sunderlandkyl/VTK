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
#include <vtkTextureObject.h>
#include <vtkTimerLog.h>
#include <vtkOpenGLShaderProperty.h>
#include <vtkUniforms.h>

#include <vtkImageToGPUImageFilter.h>
#include <vtkGPUSimpleImageFilter.h>
#include <vtkGPUImageToImageFilter.h>
#include <vtkGPUImageGaussianFilter.h>

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

  vtkNew<vtkNrrdReader> reader;
  reader->SetFileName("C:/Users/kyles/Desktop/MRHead.nrrd");
  reader->Update();
  vtkNew<vtkImageData> inputImage;
  inputImage->DeepCopy(reader->GetOutput());

  vtkNew<vtkTimerLog> timer;

  timer->StartTimer();

  vtkNew<vtkImageToGPUImageFilter> inputConvert;
  inputConvert->SetInputDataObject(inputImage);

  inputConvert->Update();
  timer->StopTimer();
  std::cout << "Input conversion took: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  vtkNew<vtkGPUSimpleImageFilter> checkerPatternGenerator;
  checkerPatternGenerator->GetShaderProperty()->SetFragmentShaderCode(cFragShader.c_str());
  checkerPatternGenerator->SetRenderWindow(inputConvert->GetRenderWindow());
  checkerPatternGenerator->SetOutputExtent(inputImage->GetExtent());
  vtkOpenGLShaderProperty* property = checkerPatternGenerator->GetShaderProperty();
  property->GetFragmentCustomUniforms()->SetUniformi("boxSize", 10);

  vtkNew<vtkGPUImageGaussianFilter> gaussianAlgorithm;
  gaussianAlgorithm->SetOutputScalarTypeToShort();
  gaussianAlgorithm->AddInputConnection(inputConvert->GetOutputPort());

  vtkNew<vtkGPUSimpleImageFilter> shaderAlgorithm;
  shaderAlgorithm->GetShaderProperty()->SetFragmentShaderCode(nFragShader.c_str());
  shaderAlgorithm->AddInputConnection(gaussianAlgorithm->GetOutputPort());
  shaderAlgorithm->AddInputConnection(checkerPatternGenerator->GetOutputPort());
  shaderAlgorithm->SetOutputScalarTypeToShort();
  shaderAlgorithm->Update();

  timer->StopTimer();
  std::cout << "Inital run took: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  vtkNew<vtkGPUImageToImageFilter> outputConvert;
  outputConvert->SetInputConnection(shaderAlgorithm->GetOutputPort());
  outputConvert->Update();

  timer->StopTimer();
  std::cout << "Output conversion took: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  property->GetFragmentCustomUniforms()->SetUniformi("boxSize", 100);
  shaderAlgorithm->Update();

  timer->StopTimer();
  std::cout << "Rerun took: " << timer->GetElapsedTime() << "s" << std::endl;
  timer->StartTimer();

  outputConvert->Update();
  vtkSmartPointer<vtkImageData> outputImage = outputConvert->GetOutput();

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