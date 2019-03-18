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

#include "vtk_glew.h"
#include "vtkGPUImageData.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkPixelBufferObject.h"
#include "vtkPixelTransfer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextureObject.h"

#include <sstream>

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
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkGPUImageData");
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUImageToImageFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
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
  vtkGPUImageData* inputGPUImage = vtkGPUImageData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (inputGPUImage)
  {
    this->RenderWindow = inputGPUImage->GetTextureObject()->GetContext();
    int outputExtent[6];
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), outputExtent);
    outputImage->SetExtent(outputExtent);
    this->Execute(inputGPUImage, outputImage);
  }

  return 1;
}

//----------------------------------------------------------------------------
void vtkGPUImageToImageFilter::Execute(vtkGPUImageData* inputGPUImage, vtkImageData* outputImage)
{
  if (!this->RenderWindow)
  {
    vtkErrorMacro("Render context not set");
  }

  vtkIdType textureDataType = inputGPUImage->GetScalarType();
  outputImage->AllocateScalars(textureDataType, inputGPUImage->GetTextureObject()->GetComponents()); // TODO: Move to texture

  vtkSmartPointer<vtkPixelBufferObject> inputPixelBuffer = vtkSmartPointer<vtkPixelBufferObject>::Take(inputGPUImage->GetTextureObject()->Download()); 
  switch (textureDataType)
  {
    vtkTemplateMacro((this->ExecuteInternal<VTK_TT>(inputPixelBuffer, outputImage)));
  }
  inputPixelBuffer->UnmapPackedBuffer();
}

//----------------------------------------------------------------------------
template<typename DATA_TYPE>
void vtkGPUImageToImageFilter::ExecuteInternal(vtkPixelBufferObject* inputPixelBuffer, vtkImageData* outputImage)
{
  DATA_TYPE* texturePointer = (DATA_TYPE*)inputPixelBuffer->MapPackedBuffer();

  int outExtent[6] = { 0, -1, 0, -1, 0, -1 };
  outputImage->GetExtent(outExtent);

  int outDimensions[3] = { 0,0,0 };
  outputImage->GetDimensions(outDimensions);

  int numberOfComponents = outputImage->GetNumberOfScalarComponents();
  DATA_TYPE* imagePointer = (DATA_TYPE*)outputImage->GetScalarPointer();

  vtkPixelExtent outputPixelExt(outExtent);
  for (int i = outExtent[4]; i <= outExtent[5]; i++)
    {
    vtkPixelTransfer::Blit<DATA_TYPE, DATA_TYPE>(
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      outputPixelExt,
      numberOfComponents,
      texturePointer,
      numberOfComponents,
      imagePointer);
    texturePointer += numberOfComponents * outDimensions[0] * outDimensions[1];
    imagePointer   += numberOfComponents * outDimensions[0] * outDimensions[1];
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
