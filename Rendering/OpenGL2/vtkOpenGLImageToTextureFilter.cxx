/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLImageToTextureFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLImageToTextureFilter.h"
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

vtkStandardNewMacro(vtkOpenGLImageToTextureFilter);

//----------------------------------------------------------------------------
vtkOpenGLImageToTextureFilter::vtkOpenGLImageToTextureFilter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->RenderWindow = nullptr;
}

//----------------------------------------------------------------------------
vtkOpenGLImageToTextureFilter::~vtkOpenGLImageToTextureFilter()
{
  this->SetRenderWindow(nullptr);
}

//----------------------------------------------------------------------------
void vtkOpenGLImageToTextureFilter::SetRenderWindow(vtkRenderWindow *renWin)
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

//----------------------------------------------------------------------------
vtkRenderWindow* vtkOpenGLImageToTextureFilter::GetRenderWindow()
{
  return this->RenderWindow;
}

//----------------------------------------------------------------------------
void vtkOpenGLImageToTextureFilter::PrintSelf(ostream& os, vtkIndent indent)
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

////----------------------------------------------------------------------------
//int vtkOpenGLImageToTextureFilter::RequestUpdateExtent(
//  vtkInformation* vtkNotUsed(request),
//  vtkInformationVector** inputVector,
//  vtkInformationVector* vtkNotUsed(outputVector))
//{
//  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
//  int ext[6];
//  inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
//  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
//  return 1;
//}

//----------------------------------------------------------------------------
int vtkOpenGLImageToTextureFilter::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLImageToTextureFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkTextureObject");
  info->Set(vtkTextureObject::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLImageToTextureFilter::ProcessRequest(vtkInformation* request,
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
int vtkOpenGLImageToTextureFilter::RequestInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // do nothing
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLImageToTextureFilter::RequestDataObject(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkTextureObject *output = vtkTextureObject::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output)
  {
    vtkTextureObject* newOutput = vtkTextureObject::New();
    outInfo->Set(vtkDataObject::DATA_OBJECT(), newOutput);
    newOutput->Delete();
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLImageToTextureFilter::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  int outputPort = request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());
  if (outputPort == -1)
  {
    outputPort = 0;
  }

  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkSmartPointer<vtkImageData> inputImage = vtkImageData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkInformation *outInfo = outputVector->GetInformationObject(outputPort);
  vtkTextureObject *outputTexture = vtkTextureObject::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  this->Execute(inputImage, outputTexture);

  return 1;
}

//----------------------------------------------------------------------------
void vtkOpenGLImageToTextureFilter::Execute(vtkImageData* inputImage, vtkTextureObject* outputTexture)
{
  // make sure it is initialized
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkSmartPointer<vtkRenderWindow>::New());
    this->RenderWindow->SetOffScreenRendering(true);
  }

  outputTexture->SetContext(this->RenderWindow);
  outputTexture->Create3DFromRaw(
    inputImage->GetDimensions()[0], inputImage->GetDimensions()[1], inputImage->GetDimensions()[2],
    inputImage->GetNumberOfScalarComponents(),
    inputImage->GetScalarType(), inputImage->GetScalarPointer());
}

//----------------------------------------------------------------------------
vtkTextureObject* vtkOpenGLImageToTextureFilter::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
vtkTextureObject* vtkOpenGLImageToTextureFilter::GetOutput(int port)
{
  return vtkTextureObject::SafeDownCast(this->GetOutputDataObject(port));
}
