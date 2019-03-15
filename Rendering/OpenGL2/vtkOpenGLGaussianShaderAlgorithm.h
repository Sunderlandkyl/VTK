/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLGaussianShaderAlgorithm.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkOpenGLGaussianShaderAlgorithm
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkOpenGLGaussianShaderAlgorithm_h
#define vtkOpenGLGaussianShaderAlgorithm_h

#include "vtkRenderingOpenGL2Module.h" // For export macro
#include "vtkOpenGLShaderAlgorithm.h"

class VTKRENDERINGOPENGL2_EXPORT vtkOpenGLGaussianShaderAlgorithm : public vtkOpenGLShaderAlgorithm
{
public:
  static vtkOpenGLGaussianShaderAlgorithm *New();
  vtkTypeMacro(vtkOpenGLGaussianShaderAlgorithm, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

 protected:
  vtkOpenGLGaussianShaderAlgorithm();
  ~vtkOpenGLGaussianShaderAlgorithm() VTK_OVERRIDE;

 private:
  vtkOpenGLGaussianShaderAlgorithm(const vtkOpenGLGaussianShaderAlgorithm&) = delete;
  void operator=(const vtkOpenGLGaussianShaderAlgorithm&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkOpenGLGaussianShaderAlgorithm.h
