/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLProgrammableShaderAlgorithm.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkOpenGLProgrammableShaderAlgorithm
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkOpenGLProgrammableShaderAlgorithm_h
#define vtkOpenGLProgrammableShaderAlgorithm_h

#include "vtkRenderingOpenGL2Module.h" // For export macro
#include "vtkObject.h"
#include "vtkAlgorithm.h"

#include <vtkOpenGLShaderAlgorithm.h>

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar

#include <vector>

class VTKRENDERINGOPENGL2_EXPORT vtkOpenGLProgrammableShaderAlgorithm : public vtkOpenGLShaderAlgorithm
{
public:
  static vtkOpenGLProgrammableShaderAlgorithm *New();
  vtkTypeMacro(vtkOpenGLProgrammableShaderAlgorithm, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkSetMacro(VertexShaderCode, std::string);
  vtkGetMacro(VertexShaderCode, std::string);

  vtkSetMacro(GeometryShaderCode, std::string);
  vtkGetMacro(GeometryShaderCode, std::string);

  vtkSetMacro(FragmentShaderCode, std::string);
  vtkGetMacro(FragmentShaderCode, std::string);

  void SetOutputExtent(int extent[6]);
  vtkGetVector6Macro(OutputExtent, int);
  void ClearOutputExtent();

 protected:
  vtkOpenGLProgrammableShaderAlgorithm();
  ~vtkOpenGLProgrammableShaderAlgorithm() VTK_OVERRIDE;

 private:
  vtkOpenGLProgrammableShaderAlgorithm(const vtkOpenGLProgrammableShaderAlgorithm&) = delete;
  void operator=(const vtkOpenGLProgrammableShaderAlgorithm&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkOpenGLProgrammableShaderAlgorithm.h
