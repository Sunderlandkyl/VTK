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
#include "vtkOpenGLShaderProperty.h"
#include "vtkOpenGLUniforms.h"

vtkStandardNewMacro(vtkOpenGLShaderAlgorithm);

// ----------------------------------------------------------------------------
vtkOpenGLShaderAlgorithm::vtkOpenGLShaderAlgorithm()
{
  this->DefaultVertexShaderSource = R"(
//VTK::System::Dec
in vec4 vertexMC;
in vec2 tcoordMC;
out vec2 tcoordVSOutput;
void main()
{
  tcoordVSOutput = tcoordMC;
  gl_Position = vertexMC;
}
)";

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

  // propagate update extent
  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  {
    return this->RequestUpdateExtent(request, inputVector, outputVector);
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
int vtkOpenGLShaderAlgorithm::RequestUpdateExtent(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    int ext[6];
    inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
    inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
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

  std::vector<vtkTextureObject*> inputTextures;
  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    vtkTextureObject* inputTexture = vtkTextureObject::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
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

  this->Execute(inputTextures, outputTexture, outExtent);

  return 1;
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::Execute(std::vector<vtkTextureObject*> inputTextures,
                                       vtkTextureObject* outputTexture,
                                       int outputExtent[6])
{
  // make sure it is initialized
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkSmartPointer<vtkRenderWindow>::New());
    this->RenderWindow->SetOffScreenRendering(true);
  }

  if (this->ShaderRebuildNeeded())
  {
    this->BuildShader(inputTextures, outputTexture);
  }

  if (!this->ShaderProgram)
  {
    //vtkErrorMacro("");
    return;
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


  // Upload the value of user-defined uniforms in the program
  auto vu = static_cast<vtkOpenGLUniforms*>(this->ShaderProperty->GetVertexCustomUniforms());
  vu->SetUniforms(this->ShaderProgram);
  auto fu = static_cast<vtkOpenGLUniforms*>(this->ShaderProperty->GetFragmentCustomUniforms());
  fu->SetUniforms(this->ShaderProgram);

  if (this->ShaderProgram != this->Quad.Program)
  {
    this->Quad.Program = this->ShaderProgram;
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

  this->UpdateTextureUniforms(inputTextures, outputTexture);

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
void vtkOpenGLShaderAlgorithm::UpdateTextureUniforms(std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture)
{
  float shift = 0.0;
  float scale = 1.0;
  outputTexture->GetShiftAndScale(shift, scale);

  // Provide shift and scale to get the data backing into its original units
  std::stringstream outputShiftSS;
  outputShiftSS << "outputShift";
  std::string outputShiftString = outputShiftSS.str();
  this->Quad.Program->SetUniformf(outputShiftString.c_str(), shift);

  std::stringstream outputScaleSS;
  outputScaleSS << "outputScale";
  std::string outputScaleString = outputScaleSS.str();
  this->Quad.Program->SetUniformf(outputScaleString.c_str(), scale);

  float outputTextureDimensions[3] = { 0,0,0 };
  outputTextureDimensions[0] = outputTexture->GetWidth();
  outputTextureDimensions[1] = outputTexture->GetHeight();
  outputTextureDimensions[2] = outputTexture->GetDepth();
  std::stringstream outputSizeSS;
  outputSizeSS << "outputSize";
  std::string outputSizeString = outputSizeSS.str();
  this->Quad.Program->SetUniform3f(outputSizeString.c_str(), outputTextureDimensions);


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

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::AddInputData(vtkTextureObject* input)
{
  this->AddInputDataInternal(0, input);
}

//----------------------------------------------------------------------------
bool vtkOpenGLShaderAlgorithm::ShaderRebuildNeeded()
{
  return (
      !this->ShaderProgram
    || this->ShaderBuildTime.GetMTime() < this->ShaderProperty->GetMTime()
    );

}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::BuildShader(std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture)
{

  std::map<vtkShader::Type, vtkShader*> shaders;
  vtkSmartPointer<vtkShader> vertexShader = vtkSmartPointer<vtkShader>::New();
  vertexShader->SetType(vtkShader::Vertex);
  shaders[vtkShader::Vertex] = vertexShader;

  vtkSmartPointer<vtkShader> fragmentShader = vtkSmartPointer<vtkShader>::New();
  fragmentShader->SetType(vtkShader::Fragment);
  shaders[vtkShader::Fragment] = fragmentShader;

  vtkSmartPointer<vtkShader> geometryShader = vtkSmartPointer<vtkShader>::New();
  geometryShader->SetType(vtkShader::Geometry);
  shaders[vtkShader::Geometry] = geometryShader;

  if (shaders[vtkShader::Vertex])
  {
    if (this->ShaderProperty->HasVertexShaderCode())
    {
      shaders[vtkShader::Vertex]->SetSource(this->ShaderProperty->GetVertexShaderCode());
    }
    else
    {
      shaders[vtkShader::Vertex]->SetSource(this->DefaultVertexShaderSource);
    }
  }

  if (shaders[vtkShader::Fragment])
  {
    if (this->ShaderProperty->HasFragmentShaderCode())
    {
      shaders[vtkShader::Fragment]->SetSource(this->ShaderProperty->GetFragmentShaderCode());
    }
    else
    {
      shaders[vtkShader::Fragment]->SetSource(this->DefaultFragmentShaderSource);
    }
  }

  //TODO
  if (shaders[vtkShader::Geometry])
  {
    shaders[vtkShader::Geometry]->SetSource(this->DefaultGeometryShaderSource);
  }

  // user specified pre replacements
  vtkOpenGLShaderProperty::ReplacementMap repMap = this->ShaderProperty->GetAllShaderReplacements();
  for (auto i : repMap)
  {
    if (i.first.ReplaceFirst)
    {
      std::string ssrc = shaders[i.first.ShaderType]->GetSource();
      vtkShaderProgram::Substitute(ssrc,
        i.first.OriginalValue,
        i.second.Replacement,
        i.second.ReplaceAll);
      shaders[i.first.ShaderType]->SetSource(ssrc);
    }
  }

  // Custom uniform variables replacements
  //---------------------------------------------------------------------------
  this->ReplaceShaderCustomUniforms(shaders, this->ShaderProperty);

  // Custom uniform variables replacements
  //---------------------------------------------------------------------------
  this->ReplaceShaderTextureInput(shaders, inputTextures, outputTexture);

  // user specified post replacements
  for (auto i : repMap)
  {
    if (!i.first.ReplaceFirst)
    {
      std::string ssrc = shaders[i.first.ShaderType]->GetSource();
      vtkShaderProgram::Substitute(ssrc,
        i.first.OriginalValue,
        i.second.Replacement,
        i.second.ReplaceAll);
      shaders[i.first.ShaderType]->SetSource(ssrc);
    }
  }

  this->ShaderProgram = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(shaders);
  if (!this->ShaderProgram)
  {
    vtkErrorMacro("Shader failed to compile");
  }
  this->ShaderBuildTime.Modified();
}

//-----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::ReplaceShaderCustomUniforms(
  std::map<vtkShader::Type, vtkShader*>& shaders, vtkOpenGLShaderProperty * p)
{
  vtkShader* vertexShader = shaders[vtkShader::Vertex];
  vtkOpenGLUniforms * vu = static_cast<vtkOpenGLUniforms*>(p->GetVertexCustomUniforms());
  vtkShaderProgram::Substitute(vertexShader,
    "//VTK::CustomUniforms::Dec",
    vu->GetDeclarations());

  vtkShader* fragmentShader = shaders[vtkShader::Fragment];
  vtkOpenGLUniforms * fu = static_cast<vtkOpenGLUniforms*>(p->GetFragmentCustomUniforms());
  vtkShaderProgram::Substitute(fragmentShader,
    "//VTK::CustomUniforms::Dec",
    fu->GetDeclarations());
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderAlgorithm::ReplaceShaderTextureInput(std::map<vtkShader::Type, vtkShader*>& shaders, std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture)
{
  std::stringstream textureUniformReplacementSS;

  textureUniformReplacementSS << "uniform vec3 outputShift;" << std::endl;
  textureUniformReplacementSS << "uniform vec3 outputScale;" << std::endl;
  textureUniformReplacementSS << "uniform vec3 outputSize;" << std::endl;

  for (int textureIndex = 0; textureIndex < inputTextures.size(); ++textureIndex)
  {
    textureUniformReplacementSS << "uniform sampler3D inputTex" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform vec3 inputShift" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform vec3 inputScale" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform vec3 inputSize" << textureIndex << ";" << std::endl;
  }

  for (std::map<vtkShader::Type, vtkShader*>::iterator shaderIt = shaders.begin(); shaderIt != shaders.end(); ++shaderIt)
  {
    vtkShader* shader = shaderIt->second;
    vtkShaderProgram::Substitute(shader,
      "//VTK::AlgTexUniforms::Dec",
      textureUniformReplacementSS.str());
  }
}