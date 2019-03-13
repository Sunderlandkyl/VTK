/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLImageAlgorithmHelper2.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkOpenGLImageAlgorithmHelper2
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkOpenGLImageAlgorithmHelper2_h
#define vtkOpenGLImageAlgorithmHelper2_h

#include "vtkRenderingOpenGL2Module.h" // For export macro
#include "vtkObject.h"

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar
#include <vector>

class vtkOpenGLRenderWindow;
class vtkRenderWindow;
class vtkImageData;
class vtkDataArray;

class vtkOpenGLImageAlgorithmCallback
{
public:
  virtual void InitializeShaderUniforms(vtkShaderProgram * /* program */) {};
  virtual void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};
  virtual ~vtkOpenGLImageAlgorithmCallback() {};
  vtkOpenGLImageAlgorithmCallback() {};
private:
  vtkOpenGLImageAlgorithmCallback(const vtkOpenGLImageAlgorithmCallback&) = delete;
  void operator=(const vtkOpenGLImageAlgorithmCallback&) = delete;
};

class VTKRENDERINGOPENGL2_EXPORT vtkOpenGLImageAlgorithmHelper2 : public vtkObject
{
public:
  static vtkOpenGLImageAlgorithmHelper2 *New();
  vtkTypeMacro(vtkOpenGLImageAlgorithmHelper2,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  struct ShaderProperties
  {
    std::string Vertex;
    std::string Geometry;
    std::string Fragment;
    vtkOpenGLImageAlgorithmCallback* Callback;
    ShaderProperties(std::string v, std::string g, std::string f, vtkOpenGLImageAlgorithmCallback* cb)
      : Vertex(v)
      , Geometry(g)
      , Fragment(f)
      , Callback(cb)
    {
    }
  };

  void Execute(
    std::vector<vtkImageData*> inputImages,
    std::vector<ShaderProperties> code,
    vtkImageData* outputImage
  );

  /**
   * Set the render window to get the OpenGL resources from
   */
  void SetRenderWindow(vtkRenderWindow *renWin);

 protected:
  vtkOpenGLImageAlgorithmHelper2();
  ~vtkOpenGLImageAlgorithmHelper2() override;

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkOpenGLHelper Quad;

 private:
  vtkOpenGLImageAlgorithmHelper2(const vtkOpenGLImageAlgorithmHelper2&) = delete;
  void operator=(const vtkOpenGLImageAlgorithmHelper2&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkOpenGLImageAlgorithmHelper2.h
