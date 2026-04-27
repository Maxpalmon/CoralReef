#pragma once

#include "CoreMinimal.h"

struct FVoxelOctreeNode {
	FVoxelOctreeNode* Children[8];
	int32 SpeciesID;
	bool bIsOccupied;
	bool bIsLeaf;

	FVoxelOctreeNode() : SpeciesID(-1), bIsOccupied(false), bIsLeaf(true) {
		for (int32 i = 0; i < 8; ++i) Children[i] = nullptr;
	}

	~FVoxelOctreeNode() {
		for (int32 i = 0; i < 8; ++i) {
			if (Children[i]) delete Children[i];
		}
	}
};

class CORALREEFGEN_API FVoxelOctree {
public:
	FVoxelOctree(int32 InInitialSize);
	~FVoxelOctree();

	void SetVoxel(FIntVector GlobalPos, int32 SpeciesID);
	int32 GetSpeciesAt(FIntVector GlobalPos) const;
	bool IsOccupied(FIntVector GlobalPos) const;

private:
	FVoxelOctreeNode* Root;
	FIntVector TreeOffset;
	int32 CurrentWorldSize;
	int32 MaxDepth;

	void ExpandTree(FIntVector TargetPos);
	bool IsInside(FIntVector GlobalPos) const;
	int32 GetOldRootIndexInNewRoot(const FIntVector& OldOffset, const FIntVector& NewOffset) const;
	int32 GetOctantIndex(const FIntVector& Target, const FIntVector& MinBound, int32 Size) const;
    
	void InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID);
};