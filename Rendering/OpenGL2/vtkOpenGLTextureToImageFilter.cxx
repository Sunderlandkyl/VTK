/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLTextureToImageFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLTextureToImageFilter.h"
#include "vtkObjectFactory.h"
#include "vtkTextureObject.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLState.h"
#include "vtk_glew.h"
#include "vtkPixelTransfer.h"
#include "vtkPointData.h"
#include "vtkPixelBufferObject.h"
#include "vtkShaderProgram.h"
#include "vtkOpenGLVertexArrayObject.h"
#include <sstream>
#include "vtkInformation.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkInformationVector.h"
#include "vtkErrorCode.h"
#include "vtkInformationObjectBaseKey.h"
//#include "vtkImageShiftScale.h:

vtkStandardNewMacro(vtkOpenGLTextureToImageFilter);

// ----------------------------------------------------------------------------
vtkOpenGLTextureToImageFilter::vtkOpenGLTextureToImageFilter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->RenderWindow = nullptr;
  this->OutputScalarType = VTK_FLOAT;
}

// ----------------------------------------------------------------------------
vtkOpenGLTextureToImageFilter::~vtkOpenGLTextureToImageFilter()
{
  this->SetRenderWindow(nullptr);
}

// ----------------------------------------------------------------------------
void vtkOpenGLTextureToImageFilter::SetRenderWindow(vtkRenderWindow *renWin)
{
  if (renWin == this->RenderWindow.GetPointer())
  {
    return;
  }

  vtkOpenGLRenderWindow *orw  = nullptr;
  if (renWin)
  {
    orw = vtkOpenGLRenderWindow::SafeDownCast(renWin);
  }

  this->RenderWindow = orw;
  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkOpenGLTextureToImageFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RenderWindow:";
  if(this->RenderWindow != nullptr)
  {
    this->RenderWindow->PrintSelf(os,indent);
  }
  else
  {
    os << "(none)" <<endl;
  }
}

// ----------------------------------------------------------------------------
int vtkOpenGLTextureToImageFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTextureObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLTextureToImageFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  info->Set(vtkTextureObject::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLTextureToImageFilter::ProcessRequest(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{

  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    return this->RequestDataObject(request, inputVector, outputVector);
  }

  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
  {
    return this->RequestData(request, inputVector, outputVector);
  }

  // execute information
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
  {
    return this->RequestInformation(request, inputVector, outputVector);
  }

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkOpenGLTextureToImageFilter::RequestInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // do nothing
  return 1;
}

//----------------------------------------------------------------------------
// Always create multiblock, although it is necessary only with Threading enabled
int vtkOpenGLTextureToImageFilter::RequestDataObject(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkImageData *output = vtkImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output)
  {
    vtkImageData* newOutput = vtkImageData::New();
    outInfo->Set(vtkDataObject::DATA_OBJECT(), newOutput);
    newOutput->Delete();
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLTextureToImageFilter::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{

  int outputPort = request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());
  if (outputPort == -1)
  {
    outputPort = 0;
  }

  vtkInformation *outInfo = outputVector->GetInformationObject(outputPort);
  vtkImageData *outputImage = vtkImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkTextureObject* inputTexture = vtkTextureObject::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  this->RenderWindow = inputTexture->GetContext();

  this->Execute(inputTexture, outputImage);

  return 1;
}

//----------------------------------------------------------------------------
void vtkOpenGLTextureToImageFilter::Execute(vtkTextureObject* inputTexture, vtkImageData* outputImage)
{
  if (!this->RenderWindow)
  {
    vtkErrorMacro("Render context not set");
  }

  vtkIdType textureDataType = inputTexture->GetVTKDataType();
  vtkSmartPointer<vtkPixelBufferObject> inputPixelBuffer = vtkSmartPointer<vtkPixelBufferObject>::Take(inputTexture->Download());

  int outExtent[6] = { 0, 255, 0, 255, 0, 129 };
  outputImage->SetExtent(outExtent);
  //outputImage->AllocateScalars(this->OutputScalarType, 1);
  outputImage->AllocateScalars(textureDataType, 1);
  void* imagePointer = outputImage->GetScalarPointer(outExtent[0], outExtent[2], outExtent[4]);

  switch (vtkTemplate2PackMacro(textureDataType, textureDataType))
  {
    vtkTemplate2Macro((this->ExecuteInternal<VTK_T1, VTK_T2>(inputTexture, inputPixelBuffer, outputImage, outExtent)));
  }
  inputPixelBuffer->UnmapPackedBuffer();
}

//----------------------------------------------------------------------------
template<typename INPUT_TYPE, typename OUTPUT_TYPE>
void vtkOpenGLTextureToImageFilter::ExecuteInternal(vtkTextureObject* inputTexture, vtkPixelBufferObject* inputPixelBuffer, vtkImageData* outputImage, int outExtent[6])
{
  INPUT_TYPE* texturePointer = (INPUT_TYPE*)inputPixelBuffer->MapPackedBuffer();
  int numberOfTextureComponents = inputPixelBuffer->GetComponents();

  OUTPUT_TYPE* imagePointer = (OUTPUT_TYPE*)outputImage->GetScalarPointer(outExtent[0], outExtent[2], outExtent[4]);
  int numberOfImageComponents = outputImage->GetNumberOfScalarComponents();

  int outDimensions[3] = { 256,256,130 };

  vtkPixelExtent outputPixelExt(outExtent);
  for (int i = outExtent[4]; i <= outExtent[5]; i++)
    {
    vtkPixelTransfer::Blit<INPUT_TYPE, OUTPUT_TYPE>(
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      numberOfTextureComponents,
      texturePointer,
      numberOfImageComponents,
      imagePointer);
    texturePointer += numberOfTextureComponents * outDimensions[0] * outDimensions[1];
    imagePointer += numberOfImageComponents * outDimensions[0] * outDimensions[1];
    }
}

//----------------------------------------------------------------------------
vtkImageData* vtkOpenGLTextureToImageFilter::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
vtkImageData* vtkOpenGLTextureToImageFilter::GetOutput(int port)
{
  return vtkImageData::SafeDownCast(this->GetOutputDataObject(port));
}
