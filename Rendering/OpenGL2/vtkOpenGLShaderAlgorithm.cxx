/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLShaderAlgorithm.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLShaderAlgorithm.h"
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

vtkStandardNewMacro(vtkOpenGLShaderAlgorithm);

// ----------------------------------------------------------------------------
vtkOpenGLShaderAlgorithm::vtkOpenGLShaderAlgorithm()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->OutputScalarType = VTK_FLOAT;
  this->RenderWindow = nullptr;

  for (int i = 0; i < 6; ++i)
  {
    this->OutputExtent[i] = 0;
    if (i % 2 == 1)
    {
      this->OutputExtent[i] = -1;
    }
  }
}

// ----------------------------------------------------------------------------
vtkOpenGLShaderAlgorithm::~vtkOpenGLShaderAlgorithm()
{
  this->SetRenderWindow(nullptr);
}

void vtkOpenGLShaderAlgorithm::SetRenderWindow(vtkRenderWindow *renWin)
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
void vtkOpenGLShaderAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
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
int vtkOpenGLShaderAlgorithm::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTextureObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLShaderAlgorithm::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkTextureObject");
  info->Set(vtkTextureObject::CONTEXT_OBJECT(), this->RenderWindow);
  return 1;
}

//----------------------------------------------------------------------------
int vtkOpenGLShaderAlgorithm::ProcessRequest(vtkInformation* request,
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
int vtkOpenGLShaderAlgorithm::RequestInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // do nothing
  return 1;
}

//----------------------------------------------------------------------------
// Always create multiblock, although it is necessary only with Threading enabled
int vtkOpenGLShaderAlgorithm::RequestDataObject(vtkInformation* vtkNotUsed(request),
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
int vtkOpenGLShaderAlgorithm::RequestData(
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
  vtkTextureObject *outputTexture = vtkTextureObject::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  std::vector<vtkSmartPointer<vtkTextureObject> > inputTextures;
  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    vtkSmartPointer<vtkTextureObject> inputTexture = vtkTextureObject::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
    inputTextures.push_back(inputTexture);
  }

  this->Execute(inputTextures, outputTexture, this->VertexShaderCode, this->GeometryShaderCode, this->FragmentShaderCode);

  return 1;
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::Execute(std::vector<vtkSmartPointer<vtkTextureObject> > inputTextures,
                                       vtkTextureObject* outputTexture,
                                       std::string vertexShaderCode,
                                       std::string geometryShaderCode,
                                       std::string fragmentShaderCode)
{
  // make sure it is initialized
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkRenderWindow::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->UnRegister(this);
  }
  this->RenderWindow->Initialize();

  // could do shortcut here if the input volume is
  // exactly what we want (updateExtent == wholeExtent)
  // vtkIdType incX, incY, incZ;
  // inImage->GetContinuousIncrements(inArray, extent, incX, incY, incZ);
  //  tmpImage->CopyAndCastFrom(inImage, inUpdateExtent)

  // now create the framebuffer for the output
  int outputDimensions[3];
  outputDimensions[0] = this->OutputExtent[1] - this->OutputExtent[0] + 1;
  outputDimensions[1] = this->OutputExtent[3] - this->OutputExtent[2] + 1;
  outputDimensions[2] = this->OutputExtent[5] - this->OutputExtent[4] + 1;  

  outputTexture->SetContext(this->RenderWindow);
  outputTexture->Create3D(outputDimensions[0], outputDimensions[1], outputDimensions[2], 4, outputTexture->GetDataType(), false);

  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState *ostate = this->RenderWindow->GetState();
  fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputTexture, 0);

  // because the same FBO can be used in another pass but with several color
  // buffers, force this pass to use 1, to avoid side effects from the
  // render of the previous frame.
  fbo->ActivateDrawBuffer(0);

  fbo->StartNonOrtho(outputDimensions[0], outputDimensions[1]);
  ostate->vtkglViewport(0, 0, outputDimensions[0], outputDimensions[1]);
  ostate->vtkglScissor(0, 0, outputDimensions[0], outputDimensions[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);

  vtkShaderProgram *prog = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
    this->VertexShaderCode.c_str(),
    this->FragmentShaderCode.c_str(),
    this->GeometryShaderCode.c_str());
  if (prog != this->Quad.Program)
  {
    this->Quad.Program = prog;
    this->Quad.VAO->ShaderProgramChanged();
  }

  for (int textureIndex = 0; textureIndex < inputTextures.size(); ++textureIndex)
  {
    float shift = 0.0;
    float scale = 1.0;
    inputTextures[textureIndex]->GetShiftAndScale(shift, scale);
    inputTextures[textureIndex]->Activate();
    int inputTexId = inputTextures[textureIndex]->GetTextureUnit();
    std::stringstream texIdSS;
    texIdSS << "inputTex" << textureIndex;
    std::string texId = texIdSS.str();
    this->Quad.Program->SetUniformi(texId.c_str(), inputTexId);

    // shift and scale to get the data backing into its original units
    std::stringstream inputShiftSS;
    inputShiftSS << "inputShift" << textureIndex;
    std::string inputShiftString = inputShiftSS.str();
    this->Quad.Program->SetUniformf(inputShiftString.c_str(), shift);

    std::stringstream inputScaleSS;
    inputScaleSS << "inputScale" << textureIndex;
    std::string inputScaleString = inputScaleSS.str();
    this->Quad.Program->SetUniformf(inputScaleString.c_str(), scale);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  // for each zslice in the output
  vtkPixelExtent outputPixelExt(this->OutputExtent);
  for (int i = this->OutputExtent[4]; i <= this->OutputExtent[5]; i++)
  {
    this->Quad.Program->SetUniformf("zPos", (i - this->OutputExtent[4] + 0.5) / (outputDimensions[2]));

    fbo->RemoveColorAttachment(fbo->GetDrawMode(), 0);
    fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputTexture, i);
    fbo->RenderQuad(
      0, outputDimensions[0] - 1,
      0, outputDimensions[1] - 1,
      this->Quad.Program, this->Quad.VAO);
  }
}
