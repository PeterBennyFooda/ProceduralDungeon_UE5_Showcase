// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * A 3D grid class for dungeon generation
 */
template<class T>
class NETWORKINGPROTOTYPE_API Grid3D
{
public:
	Grid3D();
	Grid3D(const FVector& size, const float& borderOffset, const int& m_unit);
	T& operator[](const FVector& pos);

	bool InBounds(const FVector& pos) const;
	bool InBoundsIgnoreOffset(const FVector& pos) const;
	FVector GetIndex(const FVector& pos) const;
	FVector GetSize() const;

private:
	FVector size;
	int unit = 1;
	float borderOffset = 0.0f;
	TArray<TArray<TArray<T>>> data;
};

// Default constructor
template <class T>
Grid3D<T>::Grid3D() : size(FVector(1, 1, 1)), borderOffset(0.0f), unit(1)
{
	int depth = FMath::RoundToInt(size.Z + 1);
	int rows = FMath::RoundToInt(size.Y + 1);
	int columns = FMath::RoundToInt(size.X + 1);
	
	data.SetNum(depth);
	for (int32 Z = 0; Z < depth; ++Z)
	{
		data[Z].SetNum(rows);
    
		for (int32 Y = 0; Y < rows; ++Y)
		{
			data[Z][Y].SetNum(columns);
		}
	}
}

// Constructor
template <class T>
Grid3D<T>::Grid3D(const FVector& m_size, const float& borderOffset, const int& m_unit) : size(m_size), unit(m_unit), borderOffset(borderOffset)
{
	if(size.X <= 0 || size.Y<=0 || size.X<=0)
	{
		//ensureMsgf(size.X > 0 && size.Y>0 && size.X>0, TEXT("BRUH why the size is 0 or negative!?"));
		UE_LOG(LogTemp, Error, TEXT("BRUH why the size is 0 or negative!?"));
		return;
	}
	
	int depth = FMath::RoundToInt(size.Z+ 1);
	depth = depth / m_unit;
	int rows = FMath::RoundToInt(size.Y+ 1);
	rows = rows / m_unit;
	int columns = FMath::RoundToInt(size.X+ 1);
	columns = columns / m_unit;
	
	data.SetNum(depth);
	for (int32 Z = 0; Z < depth; ++Z)
	{
		data[Z].SetNum(rows);
    
		for (int32 Y = 0; Y < rows; ++Y)
		{
			data[Z][Y].SetNum(columns);
		}
	}
}

// Bracket operator
template <class T>
T& Grid3D<T>::operator[](const FVector& pos)
{
	FVector index = GetIndex(pos);
	return data[index.Z][index.Y][index.X];
}

/*
 * @brief Check if a position is within the bounds of the 3D grid
 * @param FVector position
 * @return bool inBounds
 */
template <class T>
bool Grid3D<T>::InBounds(const FVector& pos) const
{
	FVector max = size - FVector(borderOffset, borderOffset, borderOffset);
	FBox tmp = FBox(FVector(borderOffset, borderOffset, borderOffset), max);
	
	return tmp.IsInside(pos);
}

/*
 * @brief Check if a position is within the bounds of the 3D grid
 * @param FVector position
 * @return bool inBounds
 */
template <class T>
bool Grid3D<T>::InBoundsIgnoreOffset(const FVector& pos) const
{
	FVector max = size;
	FBox tmp = FBox(FVector(0, 0, 0), max);
	
	return tmp.IsInside(pos);
}

/*
 * @brief Get the index of a position in the 3D grid
 * @param FVector position
 * @return FVector index
 */
template <class T>
FVector Grid3D<T>::GetIndex(const FVector& pos) const
{
	int X = FMath::RoundToInt(pos.X) / unit;
	int Y = FMath::RoundToInt(pos.Y) / unit;
	int Z = FMath::RoundToInt(pos.Z) / unit;
	return FVector(X, Y, Z);
}

/*
 *	@brief Get the size of the 3D grid
 *	@return FVector size
 */
template <class T>
FVector Grid3D<T>::GetSize() const
{
	return size;
}
