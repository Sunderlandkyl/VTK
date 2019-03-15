/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLGaussianShaderAlgorithm.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLGaussianShaderAlgorithm.h"
#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkOpenGLGaussianShaderAlgorithm);

// ----------------------------------------------------------------------------
vtkOpenGLGaussianShaderAlgorithm::vtkOpenGLGaussianShaderAlgorithm()
{
  this->FragmentShaderCode = R"(
    //VTK::System::Dec
    varying vec2 tcoordVSOutput;
    uniform float zPos;
    uniform sampler3D inputTex0;
    uniform float inputShift0;
    uniform float inputScale0;
    //VTK::Output::Dec

    float normpdf(float x, float sigma)
    {
      return 0.39894*exp(-0.5*x*x / (sigma*sigma)) / sigma;
    }

    void main()
    {
      vec3 resolution = vec3(256, 256, 130);

      //declare stuff
      const int mSize = 11;
      const int kSize = (mSize - 1) / 2;
      float kernel[mSize];
      vec3 final_colour = vec3(0.0);

      //create the 1-D kernel
      float sigma = 7.0;
      float Z = 0.0;
      for (int j = 0; j <= kSize; ++j)
      {
        kernel[kSize + j] = kernel[kSize - j] = normpdf(float(j), sigma);
      }

      //get the normalization factor (as the gaussian has been clamped)
      for (int j = 0; j < mSize; ++j)
      {
        Z += kernel[j];
      }

      //read out the texels
      for (int i = -kSize; i <= kSize; ++i)
      {
        for (int j = -kSize; j <= kSize; ++j)
        {
          for (int k = -kSize; k <= kSize; ++k)
          {
            final_colour +=
              kernel[kSize + j] * kernel[kSize + i] * kernel[kSize + k]
              * texture3D(inputTex0,
                vec3(tcoordVSOutput, zPos) + vec3(float(i) / resolution.x, float(j) / resolution.y, float(k) / resolution.z)).rgb;
          }
        }
      }

      gl_FragData[0] = vec4(final_colour / (Z*Z*Z), 1.0);
    };)";
}

// ----------------------------------------------------------------------------
vtkOpenGLGaussianShaderAlgorithm::~vtkOpenGLGaussianShaderAlgorithm()
{
}

// ----------------------------------------------------------------------------
void vtkOpenGLGaussianShaderAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
