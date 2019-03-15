/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLShaderAlgorithm.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkOpenGLShaderAlgorithm
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkOpenGLShaderAlgorithm_h
#define vtkOpenGLShaderAlgorithm_h

#include "vtkRenderingOpenGL2Module.h" // For export macro
#include "vtkObject.h"
#include "vtkAlgorithm.h"
#include "vtkShader.h"
#include <map>

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar

#include <vector>

class vtkOpenGLRenderWindow;
class vtkRenderWindow;
class vtkDataArray;
class vtkTextureObject;
class vtkOpenGLShaderProperty;

class vtkOpenGLImageAlgorithmCallback
{
public:
  void InitializeShaderUniforms(vtkShaderProgram * /* program */) {};
  void UpdateShaderUniforms(
    vtkShaderProgram * /* program */, int /* zExtent */) {};
  ~vtkOpenGLImageAlgorithmCallback() {};
  vtkOpenGLImageAlgorithmCallback() {};
private:
  vtkOpenGLImageAlgorithmCallback(const vtkOpenGLImageAlgorithmCallback&) = delete;
  void operator=(const vtkOpenGLImageAlgorithmCallback&) = delete;
};

class VTKRENDERINGOPENGL2_EXPORT vtkOpenGLShaderAlgorithm : public vtkAlgorithm
{
public:
  static vtkOpenGLShaderAlgorithm *New();
  vtkTypeMacro(vtkOpenGLShaderAlgorithm, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
  * Get the output data object for a port on this algorithm.
  */
  vtkTextureObject* GetOutput();
  vtkTextureObject* GetOutput(int);
  void SetOutput(vtkDataObject* d);
  //@}

  /**
  * Process a request from the executive.  For vtkImageAlgorithm, the
  * request will be delegated to one of the following methods: RequestData,
  * RequestInformation, or RequestUpdateExtent.
  */
  int ProcessRequest(vtkInformation*,
                     vtkInformationVector**,
                     vtkInformationVector*) VTK_OVERRIDE;

  /**
  * Subclasses can reimplement this method to translate the update
  * extent requests from each output port into update extent requests
  * for the input connections.
  */
  virtual int RequestUpdateExtent(vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*);

  //@{
  /**
  * Assign a data object as input. Note that this method does not
  * establish a pipeline connection. Use SetInputConnection to
  * setup a pipeline connection.
  */
  //void SetInputData(int index, vtkDataObject *);
  //void SetInputData(vtkDataObject *input) { this->SetInputData(0, input); };
  //@}

  //@{
  /**
  * Get a data object for one of the input port connections.  The use
  * of this method is strongly discouraged, but some filters that were
  * written a long time ago still use this method.
  */
  vtkTextureObject *GetInput(int index);
  //@}

  //@{
  /**
  * Assign a data object as input. Note that this method does not
  * establish a pipeline connection. Use SetInputConnection to
  * setup a pipeline connection.
  */
  void AddInputData(vtkTextureObject *);

  //@}

  /**
   * Set the render window to get the OpenGL resources from
   */
  void SetRenderWindow(vtkRenderWindow *renWin);

  //@{
  /**
  * Set the desired output scalar type. The result of the shift
  * and scale operations is cast to the type specified.
  */
  vtkSetMacro(OutputScalarType, int);
  vtkGetMacro(OutputScalarType, int);
  void SetOutputScalarTypeToDouble()
  {
    this->SetOutputScalarType(VTK_DOUBLE);
  }
  void SetOutputScalarTypeToFloat()
  {
    this->SetOutputScalarType(VTK_FLOAT);
  }
  void SetOutputScalarTypeToLong()
  {
    this->SetOutputScalarType(VTK_LONG);
  }
  void SetOutputScalarTypeToUnsignedLong()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_LONG);
  };
  void SetOutputScalarTypeToInt()
  {
    this->SetOutputScalarType(VTK_INT);
  }
  void SetOutputScalarTypeToUnsignedInt()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_INT);
  }
  void SetOutputScalarTypeToShort()
  {
    this->SetOutputScalarType(VTK_SHORT);
  }
  void SetOutputScalarTypeToUnsignedShort()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_SHORT);
  }
  void SetOutputScalarTypeToChar()
  {
    this->SetOutputScalarType(VTK_CHAR);
  }
  void SetOutputScalarTypeToUnsignedChar()
  {
    this->SetOutputScalarType(VTK_UNSIGNED_CHAR);
  }
  //@}

 protected:
  vtkOpenGLShaderAlgorithm();
  ~vtkOpenGLShaderAlgorithm() VTK_OVERRIDE;

  int RequestInformation(vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  // Pipeline-related methods
  int RequestDataObject(vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*);

  /**
   * This is called in response to a REQUEST_DATA request from the
   * executive.  Subclasses should override either this method or the
   * ExecuteDataWithInformation method in order to generate data for
   * their outputs.  For images, the output arrays will already be
   * allocated, so all that is necessary is to fill in the voxel values.
   */
  virtual int RequestData(vtkInformation *request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);

  bool ShaderRebuildNeeded();
  void BuildShader(std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture);

  void Execute(std::vector<vtkTextureObject*> inputTextures,
               vtkTextureObject* outputTexture,
               int outputExtent[6]);

  void ReplaceShaderCustomUniforms(std::map<vtkShader::Type, vtkShader*>& shaders, vtkOpenGLShaderProperty * p);
  void ReplaceShaderTextureInput(std::map<vtkShader::Type, vtkShader*>& shaders, std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture);

  void UpdateTextureUniforms(std::vector<vtkTextureObject*> inputTextures, vtkTextureObject* outputTexture);

  // see vtkAlgorithm for docs.
  int FillInputPortInformation(int, vtkInformation*) override;
  int FillOutputPortInformation(int, vtkInformation*) override;

  vtkSmartPointer<vtkOpenGLRenderWindow> RenderWindow;
  vtkNew<vtkOpenGLShaderProperty> ShaderProperty;

  vtkOpenGLHelper Quad;

  int OutputScalarType;
  int OutputExtent[6];
  bool OutputExtentSpecified;

  std::string DefaultVertexShaderSource;
  std::string DefaultFragmentShaderSource;
  std::string DefaultGeometryShaderSource;

  vtkShaderProgram* ShaderProgram;
  vtkTimeStamp ShaderBuildTime;

 private:
  vtkOpenGLShaderAlgorithm(const vtkOpenGLShaderAlgorithm&) = delete;
  void operator=(const vtkOpenGLShaderAlgorithm&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkOpenGLShaderAlgorithm.h
