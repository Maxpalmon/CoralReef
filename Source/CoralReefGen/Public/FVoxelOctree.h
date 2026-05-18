#pragma once

#include "CoreMinimal.h"

struct FSpeciesParams; 

struct FVoxelOctreeNode {
	FVoxelOctreeNode* Children[8];
	int32 SpeciesID;
	int32 InstanceIndex; // Храним индекс меша для перекрашивания
	bool bIsOccupied;
	bool bIsLeaf;

	FVoxelOctreeNode() : SpeciesID(-1), InstanceIndex(-1), bIsOccupied(false), bIsLeaf(true) {
		for (int32 i = 0; i < 8; ++i) Children[i] = nullptr;
	}

	~FVoxelOctreeNode() {
		for (int32 i = 0; i < 8; ++i) {
			if (Children[i]) delete Children[i];
		}
	}
};

class FVoxelOctree {
public:
	FVoxelOctree(int32 InInitialSize);
	~FVoxelOctree();

	// Добавлен аргумент InInstanceIndex
	void SetVoxel(FIntVector GlobalPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList, class UInstancedStaticMeshComponent* MeshComp, int32 InInstanceIndex = -1);
    
	int32 GetSpeciesAt(FIntVector GlobalPos) const;
	bool IsOccupied(FIntVector GlobalPos) const;
	int32 GetTotalNodesCount() const;
	int32 GetCurrentSize() const { return CurrentWorldSize; }
	
	int32 GetVoxelInstanceIndex(FIntVector GlobalPos) const;

private:
	FVoxelOctreeNode* Root;
	FIntVector TreeOffset;
	int32 CurrentWorldSize;
	int32 MaxDepth;
	

	void ExpandTree(FIntVector TargetPos);
	bool IsInside(FIntVector GlobalPos) const;
	int32 GetOctantIndex(const FIntVector& Target, const FIntVector& MinBound, int32 Size) const;
	int32 CountNodesRecursive(FVoxelOctreeNode* Node) const;
    
	void InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList, class UInstancedStaticMeshComponent* MeshComp, int32 InInstanceIndex);
};