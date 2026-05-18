#include "FVoxelOctree.h"
#include "CoralGrowthManager.h"

FVoxelOctree::FVoxelOctree(int32 InInitialSize) {
    CurrentWorldSize = InInitialSize;
    TreeOffset = FIntVector(-InInitialSize / 2); 
    MaxDepth = FMath::CeilLogTwo(InInitialSize);
    Root = new FVoxelOctreeNode();
}

FVoxelOctree::~FVoxelOctree() {
    delete Root;
}

void FVoxelOctree::SetVoxel(FIntVector GlobalPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList, UInstancedStaticMeshComponent* MeshComp, int32 InInstanceIndex) {
    while (!IsInside(GlobalPos)) {
        ExpandTree(GlobalPos);
    }
    InsertRecursive(Root, TreeOffset, CurrentWorldSize, 0, GlobalPos, SpeciesID, SpeciesList, MeshComp, InInstanceIndex);
}

void FVoxelOctree::InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList, UInstancedStaticMeshComponent* MeshComp, int32 InInstanceIndex) {
    if (Depth == MaxDepth) {
        if (Node->bIsOccupied) {
            int32 CurrentAggro = SpeciesList.IsValidIndex(Node->SpeciesID) ? SpeciesList[Node->SpeciesID].AggressionLevel : 0;
            int32 NewAggro = SpeciesList.IsValidIndex(SpeciesID) ? SpeciesList[SpeciesID].AggressionLevel : 0;

            if (NewAggro > CurrentAggro) {
                Node->SpeciesID = SpeciesID;
                
                if (MeshComp && Node->InstanceIndex != -1) {
                    FColor NewCol = SpeciesList[SpeciesID].DisplayColor;
                    MeshComp->SetCustomDataValue(Node->InstanceIndex, 0, NewCol.R / 255.0f);
                    MeshComp->SetCustomDataValue(Node->InstanceIndex, 1, NewCol.G / 255.0f);
                    MeshComp->SetCustomDataValue(Node->InstanceIndex, 2, NewCol.B / 255.0f);
                }
            }
        } else {
            Node->bIsOccupied = true;
            Node->SpeciesID = SpeciesID;
            Node->InstanceIndex = InInstanceIndex;
        }
        return;
    }

    int32 Half = Size / 2;
    int32 ChildIdx = GetOctantIndex(TargetPos, NodePos, Half);

    if (!Node->Children[ChildIdx]) {
        Node->Children[ChildIdx] = new FVoxelOctreeNode();
    }

    FIntVector ChildNodePos = NodePos;
    if (ChildIdx & 1) ChildNodePos.X += Half;
    if (ChildIdx & 2) ChildNodePos.Y += Half;
    if (ChildIdx & 4) ChildNodePos.Z += Half;

    InsertRecursive(Node->Children[ChildIdx], ChildNodePos, Half, Depth + 1, TargetPos, SpeciesID, SpeciesList, MeshComp, InInstanceIndex);
}

int32 FVoxelOctree::GetVoxelInstanceIndex(FIntVector GlobalPos) const {
    if (!IsInside(GlobalPos)) return -1;
    FVoxelOctreeNode* Current = Root;
    FIntVector NodePos = TreeOffset;
    int32 Size = CurrentWorldSize;

    for (int32 d = 0; d < MaxDepth; ++d) {
        int32 Half = Size / 2;
        int32 ChildIdx = GetOctantIndex(GlobalPos, NodePos, Half);
        if (!Current->Children[ChildIdx]) return -1;
        Current = Current->Children[ChildIdx];
        if (ChildIdx & 1) NodePos.X += Half;
        if (ChildIdx & 2) NodePos.Y += Half;
        if (ChildIdx & 4) NodePos.Z += Half;
        Size = Half;
    }
    return Current->bIsOccupied ? Current->InstanceIndex : -1;
}

int32 FVoxelOctree::GetSpeciesAt(FIntVector GlobalPos) const {
    if (!IsInside(GlobalPos)) return -1;
    FVoxelOctreeNode* Current = Root;
    FIntVector NodePos = TreeOffset;
    int32 Size = CurrentWorldSize;

    for (int32 d = 0; d < MaxDepth; ++d) {
        int32 Half = Size / 2;
        int32 ChildIdx = GetOctantIndex(GlobalPos, NodePos, Half);
        if (!Current->Children[ChildIdx]) return -1;
        Current = Current->Children[ChildIdx];
        if (ChildIdx & 1) NodePos.X += Half;
        if (ChildIdx & 2) NodePos.Y += Half;
        if (ChildIdx & 4) NodePos.Z += Half;
        Size = Half;
    }
    return Current->bIsOccupied ? Current->SpeciesID : -1;
}

bool FVoxelOctree::IsOccupied(FIntVector GlobalPos) const {
    return GetSpeciesAt(GlobalPos) != -1;
}

bool FVoxelOctree::IsInside(FIntVector GlobalPos) const {
    return GlobalPos.X >= TreeOffset.X && GlobalPos.X < TreeOffset.X + CurrentWorldSize &&
           GlobalPos.Y >= TreeOffset.Y && GlobalPos.Y < TreeOffset.Y + CurrentWorldSize &&
           GlobalPos.Z >= TreeOffset.Z && GlobalPos.Z < TreeOffset.Z + CurrentWorldSize;
}

int32 FVoxelOctree::GetOctantIndex(const FIntVector& Target, const FIntVector& MinBound, int32 Size) const {
    int32 Index = 0;
    if (Target.X >= MinBound.X + Size) Index |= 1;
    if (Target.Y >= MinBound.Y + Size) Index |= 2;
    if (Target.Z >= MinBound.Z + Size) Index |= 4;
    return Index;
}

void FVoxelOctree::ExpandTree(FIntVector TargetPos) {
    int32 NewSize = CurrentWorldSize * 2;
    FIntVector NewOffset = TreeOffset - FIntVector(CurrentWorldSize / 2);
    FVoxelOctreeNode* NewRoot = new FVoxelOctreeNode();
    delete Root;
    Root = NewRoot;
    CurrentWorldSize = NewSize;
    TreeOffset = NewOffset;
    MaxDepth = FMath::CeilLogTwo(NewSize);
}

int32 FVoxelOctree::GetTotalNodesCount() const {
    return CountNodesRecursive(Root);
}

int32 FVoxelOctree::CountNodesRecursive(FVoxelOctreeNode* Node) const {
    if (!Node) return 0;
    int32 Count = 1; 
    
    // Безопасный обход дерева без жесткой привязки к флагу bIsLeaf
    for (int32 i = 0; i < 8; ++i) {
        if (Node->Children[i]) {
            Count += CountNodesRecursive(Node->Children[i]);
        }
    }
    return Count;
}