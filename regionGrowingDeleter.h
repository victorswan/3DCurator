#ifndef REGIONGROWINGDELETER_H
#define REGIONGROWINGDELETER_H

#include <array>
#include <queue>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

#include "segmentationCommons.h"

#define MIN_AIR -850 /**< Min air value */
#define AIR_HU -1000 /**< Air value */

/**
 * Search a seed where star the region growing
 * @param imageData	3D image data
 * @param ijk		initial coord
 * @param bounds	bounds
 * @return			seed to start the region growing
 */
Coord2D searchInitialVoxel(vtkSmartPointer<vtkImageData> imageData, const int ijk[3], const Bounds bounds);

/**
 * 2D Region growing deleter 
 * @param imageData	3D image data
 * @param ijk		initial coord
 * @param bounds	bounds
 */
void deleteImage(vtkSmartPointer<vtkImageData> imageData, const int ijk[3], const Bounds bounds);

/**
 * 3D Region growing deleter
 * @param imageData	3D image data
 * @param ijk		initial coord
 * @param bounds	bounds
 */
void deleteByImages(vtkSmartPointer<vtkImageData> imageData, const int ijk[3], const Bounds bounds);

#endif // REGIONGROWINGDELETER_H