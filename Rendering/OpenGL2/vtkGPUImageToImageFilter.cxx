/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageToImageFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkGPUImageToImageFilter.h"
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
#include "vtkStreamingDemandDrivenPipeline.h"

vtkStandardNewMacro(vtkGPUImageToImageFilter);

// ----------------------------------------------------------------------------
vtkGPUImageToImageFilter::vtkGPUImageToImageFilter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->RenderWindow = nullptr;
  this->OutputScalarType = VTK_FLOAT;
}

// ----------------------------------------------------------------------------
vtkGPUImageToImageFilter::~vtkGPUImageToImageFilter()
{
  this->SetRenderWindow(nullptr);
}

// ----------------------------------------------------------------------------
void vtkGPUImageToImageFilter::SetRenderWindow(vtkRenderWindow *renWin)
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
void vtkGPUImageToImageFilter::PrintSelf(ostream& os, vtkIndent indent)
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
int vtkGPUImageToImageFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTextureObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUImageToImageFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  info->Set(vtkTextureObject::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUImageToImageFilter::ProcessRequest(vtkInformation* request,
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

  //// propagate update extent
  //if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  //{
  //  return this->RequestUpdateExtent(request, inputVector, outputVector);
  //}

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkGPUImageToImageFilter::RequestInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // do nothing
  return 1;
}

////----------------------------------------------------------------------------
//int vtkGPUImageToImageFilter::RequestUpdateExtent(
//  vtkInformation * vtkNotUsed(request),
//  vtkInformationVector **inputVector,
//  vtkInformationVector *vtkNotUsed(outputVector))
//{
//  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
//  int ext[6];
//  inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
//  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
//  return 1;
//}

//----------------------------------------------------------------------------
int vtkGPUImageToImageFilter::RequestDataObject(vtkInformation* vtkNotUsed(request),
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
int vtkGPUImageToImageFilter::RequestData(
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

  int outputExtent[6];
  outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), outputExtent);
  this->Execute(inputTexture, outputImage, outputExtent);

  return 1;
}

//----------------------------------------------------------------------------
void vtkGPUImageToImageFilter::Execute(vtkTextureObject* inputTexture, vtkImageData* outputImage, int outputExtent[6])
{
  if (!this->RenderWindow)
  {
    vtkErrorMacro("Render context not set");
  }

  vtkIdType textureDataType = inputTexture->GetVTKDataType();
  vtkSmartPointer<vtkPixelBufferObject> inputPixelBuffer = vtkSmartPointer<vtkPixelBufferObject>::Take(inputTexture->Download());

  outputImage->SetExtent(outputExtent);
  outputImage->AllocateScalars(textureDataType, 1);

  void* imagePointer = outputImage->GetScalarPointer(outputExtent[0], outputExtent[2], outputExtent[4]);

  switch (vtkTemplate2PackMacro(textureDataType, textureDataType))
  {
    vtkTemplate2Macro((this->ExecuteInternal<VTK_T1, VTK_T2>(inputTexture, inputPixelBuffer, outputImage, outputExtent)));
  }
  inputPixelBuffer->UnmapPackedBuffer();
}

//----------------------------------------------------------------------------
template<typename INPUT_TYPE, typename OUTPUT_TYPE>
void vtkGPUImageToImageFilter::ExecuteInternal(vtkTextureObject* inputTexture, vtkPixelBufferObject* inputPixelBuffer, vtkImageData* outputImage, int outExtent[6])
{
  INPUT_TYPE* texturePointer = (INPUT_TYPE*)inputPixelBuffer->MapPackedBuffer();
  int numberOfTextureComponents = inputPixelBuffer->GetComponents();

  OUTPUT_TYPE* imagePointer = (OUTPUT_TYPE*)outputImage->GetScalarPointer(outExtent[0], outExtent[2], outExtent[4]);
  int numberOfImageComponents = outputImage->GetNumberOfScalarComponents();

  int outDimensions[3];
  outDimensions[0] = outExtent[1] - outExtent[0] + 1;
  outDimensions[1] = outExtent[3] - outExtent[2] + 1;
  outDimensions[2] = outExtent[5] - outExtent[4] + 1;

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
vtkImageData* vtkGPUImageToImageFilter::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
vtkImageData* vtkGPUImageToImageFilter::GetOutput(int port)
{
  return vtkImageData::SafeDownCast(this->GetOutputDataObject(port));
}
