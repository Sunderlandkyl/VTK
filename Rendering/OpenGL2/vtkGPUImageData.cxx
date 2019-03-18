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

#include "vtkObjectFactory.h"
#include "vtkTextureObject.h"
#include "vtkInformation.h"

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

  this->TextureObject = vtkSmartPointer<vtkTextureObject>::New();
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
  this->TextureObject->SetContext(renWin);
}

//----------------------------------------------------------------------------
vtkOpenGLRenderWindow* vtkGPUImageData::GetContext()
{
  return this->TextureObject->GetContext();
}

//----------------------------------------------------------------------------
void vtkGPUImageData::AllocateScalars(int dataType, int numComponents)
{
  int dimensions[3] = { 0,0,0 };
  this->GetDimensions(dimensions);
  this->TextureObject->Create3D(dimensions[0], dimensions[1], dimensions[2], numComponents, dataType, false);

}

//----------------------------------------------------------------------------
void vtkGPUImageData::AllocateScalarsFromPointer(int dataType, int numComponents, void *data)
{
  int dimensions[3] = { 0,0,0 };
  this->GetDimensions(dimensions);
  this->TextureObject->Create3DFromRaw(dimensions[0], dimensions[1], dimensions[2], numComponents, dataType, data);
}

//----------------------------------------------------------------------------
int vtkGPUImageData::GetScalarType()
{
  return this->TextureObject->GetVTKDataType();
}

//----------------------------------------------------------------------------
void vtkGPUImageData::CopyInformationFromPipeline(vtkInformation* info)
{
  this->CopyOriginAndSpacingFromPipeline(info);
}

//----------------------------------------------------------------------------
void vtkGPUImageData::CopyOriginAndSpacingFromPipeline(vtkInformation* info)
{
  // Copy origin and spacing from pipeline information to the internal
  // copies.
  if (info->Has(SPACING()))
  {
    this->SetSpacing(info->Get(SPACING()));
  }
  if (info->Has(ORIGIN()))
  {
    this->SetOrigin(info->Get(ORIGIN()));
  }
}