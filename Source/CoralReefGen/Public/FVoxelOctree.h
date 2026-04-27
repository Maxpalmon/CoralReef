// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
struct FVoxelOctreeNode
{
	FVoxelOctreeNode* Children[8];
	bool bIsOccupied;
	int32 SpeciesID;
	
	FVoxelOctreeNode():bIsOccupied(false), SpeciesID(-1)
	{
		for (int32 i = 0; i < 8; i++)
		{
			Children[i] = nullptr;
		}
	}
	
	~FVoxelOctreeNode()
	{
		for (int32 i = 0; i < 8; i++)
		{
			if (Children[i]) delete Children[i];
		}
	}
};

class CORALREEFGEN_API FVoxelOctree
{
public:
	FVoxelOctree();
	FVoxelOctree(int32 InWorldSize);
	~FVoxelOctree();
	
	//Recursive method for inserting
	void InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID);
	
	int32 GetOctantIndex(const FIntVector& Target, const FIntVector& NodePos, int32 HalfSize) const;
	
	void SetVoxel(FIntVector Pos, int32 SpeciesID);
	
	bool IsOccupied(FIntVector Pos) const;
	
	int32 GetSpeciesAt(FIntVector Pos) const;
	
	private:
	FVoxelOctreeNode* Root;
	int32 WorldSize;
	int32 MaxDepth;
	
	
};
