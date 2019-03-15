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
#include "vtkStreamingDemandDrivenPipeline.h"

vtkStandardNewMacro(vtkOpenGLShaderAlgorithm);

// ----------------------------------------------------------------------------
vtkOpenGLShaderAlgorithm::vtkOpenGLShaderAlgorithm()
{
  this->VertexShaderCode = R"(
    //VTK::System::Dec
    attribute vec4 vertexMC;
    attribute vec2 tcoordMC;
    varying vec2 tcoordVSOutput;
    void main()
    {
      tcoordVSOutput = tcoordMC;
      gl_Position = vertexMC;
    };)";

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->OutputScalarType = VTK_FLOAT;
  this->RenderWindow = nullptr;
  this->OutputExtentSpecified = false;
}

// ----------------------------------------------------------------------------
vtkOpenGLShaderAlgorithm::~vtkOpenGLShaderAlgorithm()
{
  this->SetRenderWindow(nullptr);
}

// ----------------------------------------------------------------------------
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

  //// propagate update extent
  //if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  //{
  //  return this->RequestUpdateExtent(request, inputVector, outputVector);
  //}

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
//int vtkOpenGLShaderAlgorithm::RequestUpdateExtent(
//  vtkInformation* vtkNotUsed(request),
//  vtkInformationVector** inputVector,
//  vtkInformationVector* outputVector)
//{
//  vtkInformation* outInfo = outputVector->GetInformationObject(0);
//  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
//  {
//    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
//    int ext[6];
//    inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
//    inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
//  }
//  return 1;
//}

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

    if (!this->RenderWindow)
    {
      this->RenderWindow = inputTexture->GetContext();
    }
  }

  if (this->OutputExtentSpecified)
  {
    outInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), this->OutputExtent, 6);
  }

  // TODO: extent calculation
  int outExtent[6];
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT()))
  {
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), outExtent);
  }

  this->Execute(inputTextures, outputTexture, this->VertexShaderCode, this->GeometryShaderCode, this->FragmentShaderCode, outExtent);

  return 1;
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::Execute(std::vector<vtkSmartPointer<vtkTextureObject> > inputTextures,
                                       vtkTextureObject* outputTexture,
                                       std::string vertexShaderCode,
                                       std::string geometryShaderCode,
                                       std::string fragmentShaderCode,
                                       int outputExtent[6])
{
  // make sure it is initialized
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkSmartPointer<vtkRenderWindow>::New());
    this->RenderWindow->SetOffScreenRendering(true);
  }

  // now create the framebuffer for the output
  int outputDimensions[3];
  outputDimensions[0] = outputExtent[1] - outputExtent[0] + 1;
  outputDimensions[1] = outputExtent[3] - outputExtent[2] + 1;
  outputDimensions[2] = outputExtent[5] - outputExtent[4] + 1;

  outputTexture->SetContext(this->RenderWindow);
  outputTexture->Create3D(outputDimensions[0], outputDimensions[1], outputDimensions[2], 4, this->OutputScalarType, false);

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

  ////////////////////////////////
  // Manage input textures
  for (int textureIndex = 0; textureIndex < inputTextures.size(); ++textureIndex)
  {
    vtkTextureObject* inputTexture = inputTextures[textureIndex];
    inputTexture->Activate();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  this->UpdateTextureUniforms(inputTextures);

  ////////////////////////////////
  // Render

  // For each zslice in the output
  vtkPixelExtent outputPixelExt(outputExtent);
  for (int i = outputExtent[4]; i <= outputExtent[5]; i++)
  {
    this->Quad.Program->SetUniformf("zPos", (i - outputExtent[4] + 0.5) / (outputDimensions[2]));

    fbo->RemoveColorAttachment(fbo->GetDrawMode(), 0);
    fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputTexture, i);
    fbo->RenderQuad(
      0, outputDimensions[0] - 1,
      0, outputDimensions[1] - 1,
      this->Quad.Program, this->Quad.VAO);
  }
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::UpdateTextureUniforms(std::vector<vtkSmartPointer<vtkTextureObject> > inputTextures)
{
  ////////////////////////////////
  // Manage input textures
  for (int textureIndex = 0; textureIndex < inputTextures.size(); ++textureIndex)
  {
    vtkTextureObject* inputTexture = inputTextures[textureIndex];
    float shift = 0.0;
    float scale = 1.0;
    inputTexture->GetShiftAndScale(shift, scale);
    int inputTexId = inputTexture->GetTextureUnit();

    // Set Texture uniforms
    std::stringstream texIdSS;
    texIdSS << "inputTex" << textureIndex;
    std::string texId = texIdSS.str();
    this->Quad.Program->SetUniformi(texId.c_str(), inputTexId);

    // Provide shift and scale to get the data backing into its original units
    std::stringstream inputShiftSS;
    inputShiftSS << "inputShift" << textureIndex;
    std::string inputShiftString = inputShiftSS.str();
    this->Quad.Program->SetUniformf(inputShiftString.c_str(), shift);

    std::stringstream inputScaleSS;
    inputScaleSS << "inputScale" << textureIndex;
    std::string inputScaleString = inputScaleSS.str();
    this->Quad.Program->SetUniformf(inputScaleString.c_str(), scale);

    float inputTextureDimensions[3] = { 0,0,0 };
    inputTextureDimensions[0] = inputTexture->GetWidth();
    inputTextureDimensions[1] = inputTexture->GetHeight();
    inputTextureDimensions[2] = inputTexture->GetDepth();
    std::stringstream inputSizeSS;
    inputSizeSS << "inputSize" << textureIndex;
    std::string inputSizeString = inputSizeSS.str();
    this->Quad.Program->SetUniform3f(inputScaleString.c_str(), inputTextureDimensions);
  }
}

//----------------------------------------------------------------------------
vtkTextureObject* vtkOpenGLShaderAlgorithm::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
vtkTextureObject* vtkOpenGLShaderAlgorithm::GetOutput(int port)
{
  return vtkTextureObject::SafeDownCast(this->GetOutputDataObject(port));
}

void vtkOpenGLShaderAlgorithm::AddInputData(vtkTextureObject* input)
{
  this->AddInputDataInternal(0, input);
}
