/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLImageAlgorithmHelper2.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLImageAlgorithmHelper2.h"
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

vtkStandardNewMacro(vtkOpenGLImageAlgorithmHelper2);

// ----------------------------------------------------------------------------
vtkOpenGLImageAlgorithmHelper2::vtkOpenGLImageAlgorithmHelper2()
{
  this->RenderWindow = nullptr;
}

// ----------------------------------------------------------------------------
vtkOpenGLImageAlgorithmHelper2::~vtkOpenGLImageAlgorithmHelper2()
{
  this->SetRenderWindow(nullptr);
}

void vtkOpenGLImageAlgorithmHelper2::SetRenderWindow(vtkRenderWindow *renWin)
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

void vtkOpenGLImageAlgorithmHelper2::Execute(
  std::vector<vtkImageData*> inputImages,
  std::vector<ShaderProperties> code,
  vtkImageData* outputImage)
{
  // make sure it is initialized
  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkRenderWindow::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->UnRegister(this);
  }
  this->RenderWindow->Initialize();

  // send vector data to a texture
  std::vector<vtkSmartPointer<vtkTextureObject> > inputTextures;
  for (std::vector<vtkImageData*>::iterator inputIt = inputImages.begin(); inputIt != inputImages.end(); ++inputIt)
  {
    int dims[3] = { 0,0,0 };
    vtkImageData* input = (*inputIt);
    input->GetDimensions(dims);

    vtkSmartPointer<vtkTextureObject> inputTex = vtkSmartPointer<vtkTextureObject>::New();
    inputTex->SetContext(this->RenderWindow);
    inputTex->Create3DFromRaw(
      dims[0], dims[1], dims[2],
      input->GetNumberOfScalarComponents(),
      input->GetScalarType(), input->GetScalarPointer());
    inputTextures.push_back(inputTex);

  }

  // could do shortcut here if the input volume is
  // exactly what we want (updateExtent == wholeExtent)
  // vtkIdType incX, incY, incZ;
  // inImage->GetContinuousIncrements(inArray, extent, incX, incY, incZ);
  //  tmpImage->CopyAndCastFrom(inImage, inUpdateExtent)

  // now create the framebuffer for the output
  int outDims[3];
  int outExt[6];
  outputImage->GetExtent(outExt);
  outDims[0] = outExt[1] - outExt[0] + 1;
  outDims[1] = outExt[3] - outExt[2] + 1;
  outDims[2] = outExt[5] - outExt[4] + 1;

  vtkNew<vtkTextureObject> outputTex;
  outputTex->SetContext(this->RenderWindow);

  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState *ostate = this->RenderWindow->GetState();

  outputTex->Create2D(outDims[0], outDims[1], 4, VTK_FLOAT, false);
  fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputTex);

  // because the same FBO can be used in another pass but with several color
  // buffers, force this pass to use 1, to avoid side effects from the
  // render of the previous frame.
  fbo->ActivateDrawBuffer(0);

  fbo->StartNonOrtho(outDims[0], outDims[1]);
  ostate->vtkglViewport(0, 0, outDims[0], outDims[1]);
  ostate->vtkglScissor(0, 0, outDims[0], outDims[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);

  for (std::vector<ShaderProperties>::iterator propertyIt = code.begin(); propertyIt != code.end(); ++propertyIt)
  {

    vtkShaderProgram *prog =
      this->RenderWindow->GetShaderCache()->ReadyShaderProgram(
        propertyIt->Vertex.c_str(), propertyIt->Fragment.c_str(), propertyIt->Geometry.c_str());
    if (prog != this->Quad.Program)
    {
      this->Quad.Program = prog;
      this->Quad.VAO->ShaderProgramChanged();
    }
    propertyIt->Callback->InitializeShaderUniforms(prog);

    for (int i = 0; i < inputTextures.size(); ++i)
    {
      float shift = 0.0;
      float scale = 1.0;
      inputTextures[i]->GetShiftAndScale(shift, scale);
      inputTextures[i]->Activate();
      int inputTexId = inputTextures[i]->GetTextureUnit();
      std::stringstream texIdSS;
      texIdSS << "inputTex" << i;
      std::string texId = texIdSS.str();
      this->Quad.Program->SetUniformi(texId.c_str(), inputTexId);

      // shift and scale to get the data backing into its original units
      std::stringstream inputShiftSS;
      inputShiftSS << "inputShift" << i;
      std::string inputShiftString = inputShiftSS.str();
      this->Quad.Program->SetUniformf(inputShiftString.c_str(), shift);

      std::stringstream inputScaleSS;
      inputScaleSS << "inputScale" << i;
      std::string inputScaleString = inputShiftSS.str();
      this->Quad.Program->SetUniformf(inputScaleString.c_str(), scale);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    fbo->AddColorAttachment(0, 0, inputTextures[0]); // Not bind input, bind output
    fbo->RemoveColorAttachment(0, 0);

    // for each zslice in the output
    vtkPixelExtent outputPixelExt(outExt);
    for (int i = outExt[4]; i <= outExt[5]; i++)
    {
      propertyIt->Callback->UpdateShaderUniforms(prog, i);
      this->Quad.Program->SetUniformf("zPos", (i - outExt[4] + 0.5) / (outDims[2]));

      fbo->RenderQuad(
        0, outDims[0] - 1,
        0, outDims[1] - 1,
        this->Quad.Program, this->Quad.VAO);

      vtkPixelBufferObject *outPBO = outputTex->Download();

      if (propertyIt == code.end() - 1)
      {
        vtkPixelTransfer::Blit<float, double>(
          outputPixelExt,
          outputPixelExt,
          outputPixelExt,
          outputPixelExt,
          4,
          (float*)outPBO->MapPackedBuffer(),
          outputImage->GetPointData()->GetScalars()->GetNumberOfComponents(),
          static_cast<double *>(outputImage->GetScalarPointer(outExt[0], outExt[2], i)));
        outPBO->UnmapPackedBuffer();
        outPBO->Delete();
      }
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
  }
}

// ----------------------------------------------------------------------------
void vtkOpenGLImageAlgorithmHelper2::PrintSelf(ostream& os, vtkIndent indent)
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
