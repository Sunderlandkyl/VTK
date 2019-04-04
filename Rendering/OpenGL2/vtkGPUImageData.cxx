/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageData.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGPUImageData.h"

#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkTextureObject.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkGPUImageData);

vtkInformationKeyMacro(vtkGPUImageData, CONTEXT_OBJECT, ObjectBase);

//----------------------------------------------------------------------------
vtkGPUImageData::vtkGPUImageData()
{
  for (int idx = 0; idx < 3; ++idx)
  {
    this->Dimensions[idx] = 0;
    this->Origin[idx] = 0.0;
    this->Spacing[idx] = 1.0;
  }

  int extent[6] = { 0, -1, 0, -1, 0, -1 };
  memcpy(this->Extent, extent, 6 * sizeof(int));

  this->Information->Set(vtkDataObject::DATA_EXTENT_TYPE(), VTK_3D_EXTENT);
  this->Information->Set(vtkDataObject::DATA_EXTENT(), this->Extent, 6);

  this->TextureObject = nullptr;
}

//----------------------------------------------------------------------------
vtkGPUImageData::~vtkGPUImageData()
{
}

//----------------------------------------------------------------------------
void vtkGPUImageData::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

}

//----------------------------------------------------------------------------
// Set dimensions of structured points dataset.
void vtkGPUImageData::SetDimensions(int i, int j, int k)
{
  this->SetExtent(0, i - 1, 0, j - 1, 0, k - 1);
}

//----------------------------------------------------------------------------
// Set dimensions of structured points dataset.
void vtkGPUImageData::SetDimensions(const int dim[3])
{
  this->SetExtent(0, dim[0] - 1, 0, dim[1] - 1, 0, dim[2] - 1);
}

//----------------------------------------------------------------------------
int *vtkGPUImageData::GetDimensions()
{
  this->GetDimensions(this->Dimensions);
  return this->Dimensions;
}

//----------------------------------------------------------------------------
void vtkGPUImageData::GetDimensions(int *dOut)
{
  const int* extent = this->Extent;
  dOut[0] = extent[1] - extent[0] + 1;
  dOut[1] = extent[3] - extent[2] + 1;
  dOut[2] = extent[5] - extent[4] + 1;
}

//----------------------------------------------------------------------------
void vtkGPUImageData::SetContext(vtkOpenGLRenderWindow* renWin)
{
  if (this->TextureObject)
  {
    this->TextureObject->SetContext(renWin);
  }
  this->Context = renWin;
}

//----------------------------------------------------------------------------
vtkOpenGLRenderWindow* vtkGPUImageData::GetContext()
{
  return this->TextureObject->GetContext();
}

//----------------------------------------------------------------------------
bool vtkGPUImageData::AllocateScalars(int dataType, int numComponents)
{
  int dimensions[3] = { 0,0,0 };
  this->GetDimensions(dimensions);
  if (this->TextureObject && (
    this->TextureObject->GetWidth() != dimensions[0] ||
    this->TextureObject->GetHeight() != dimensions[1] ||
    this->TextureObject->GetDepth() != dimensions[2] ||
    this->TextureObject->GetVTKDataType() != dataType ||
    this->TextureObject->GetComponents() != numComponents ))
  {
    this->TextureObject = nullptr;
  }

  if (!this->TextureObject)
  {
    this->TextureObject = vtkSmartPointer<vtkTextureObject>::New();
    this->TextureObject->SetContext(this->Context);
    bool textureInt = false;
    if (!this->TextureObject->Create3D(dimensions[0], dimensions[1], dimensions[2], numComponents, dataType, false))
    {
      this->TextureObject = nullptr;
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool vtkGPUImageData::AllocateScalarsFromPointer(int dataType, int numComponents, void *data)
{
  int dimensions[3] = { 0,0,0 };
  this->GetDimensions(dimensions);

  this->TextureObject = vtkSmartPointer<vtkTextureObject>::New();
  this->TextureObject->SetContext(this->Context);
  if (!this->TextureObject->Create3DFromRaw(dimensions[0], dimensions[1], dimensions[2], numComponents, dataType, data))
  {
    this->TextureObject = nullptr;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
int vtkGPUImageData::GetScalarType()
{
  return this->TextureObject->GetVTKDataType();
}

//----------------------------------------------------------------------------
void vtkGPUImageData::CopyInformationFromPipeline(vtkInformation* info)
{
  if (info->Has(vtkDataObject::SPACING()))
  {
    this->SetSpacing(info->Get(vtkDataObject::SPACING()));
  }
  if (info->Has(vtkDataObject::ORIGIN()))
  {
    this->SetOrigin(info->Get(vtkDataObject::ORIGIN()));
  }
  if (info->Has(vtkGPUImageData::CONTEXT_OBJECT()))
  {
    vtkOpenGLRenderWindow* contextObject = vtkOpenGLRenderWindow::SafeDownCast(info->Get(vtkGPUImageData::CONTEXT_OBJECT()));
    this->SetContext(contextObject);
  }
}

//----------------------------------------------------------------------------
void vtkGPUImageData::CopyInformationToPipeline(vtkInformation* info)
{
  info->Set(vtkDataObject::SPACING(), this->Spacing, 3);
  info->Set(vtkDataObject::ORIGIN(), this->Origin, 3);
  if (this->GetContext())
  {
    info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->GetContext());
  }
}