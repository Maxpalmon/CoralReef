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

void FVoxelOctree::SetVoxel(FIntVector GlobalPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList) {
    while (!IsInside(GlobalPos)) {
        ExpandTree(GlobalPos);
    }
    // ИСПРАВЛЕНО: Теперь передаем 0 (начальная глубина) и SpeciesList
    InsertRecursive(Root, TreeOffset, CurrentWorldSize, 0, GlobalPos, SpeciesID, SpeciesList);
}

int32 FVoxelOctree::GetSpeciesAt(FIntVector GlobalPos) const {
    if (!Root || !IsInside(GlobalPos)) return -1;

    FVoxelOctreeNode* Current = Root;
    FIntVector NodePos = TreeOffset;
    int32 Size = CurrentWorldSize;

    for (int32 Depth = 0; Depth < MaxDepth; ++Depth) {
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

void FVoxelOctree::ExpandTree(FIntVector TargetPos) {
    FIntVector OldOffset = TreeOffset;
    int32 OldSize = CurrentWorldSize;
    FVoxelOctreeNode* OldRoot = Root;

    // Удваиваем размер
    int32 NewSize = OldSize * 2;
    FIntVector NewOffset = OldOffset;

    // Определяем направление расширения
    if (TargetPos.X < OldOffset.X) NewOffset.X -= OldSize;
    if (TargetPos.Y < OldOffset.Y) NewOffset.Y -= OldSize;
    if (TargetPos.Z < OldOffset.Z) NewOffset.Z -= OldSize;

    // Создаем новый корень (теперь он не лист)
    Root = new FVoxelOctreeNode();
    Root->bIsLeaf = false;

    // Помещаем старое дерево в один из новых октантов
    int32 OldRootIdx = GetOldRootIndexInNewRoot(OldOffset, NewOffset);
    Root->Children[OldRootIdx] = OldRoot;

    // Обновляем параметры
    CurrentWorldSize = NewSize;
    TreeOffset = NewOffset;
    MaxDepth++; 
}

void FVoxelOctree::InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID, const TArray<FSpeciesParams>& SpeciesList) {
    if (Depth == MaxDepth) {
        if (Node->bIsOccupied) {
            // МЕЖВИДОВАЯ БОРЬБА: Сравниваем AggressionLevel из твоего БП
            int32 CurrentAggro = SpeciesList.IsValidIndex(Node->SpeciesID) ? SpeciesList[Node->SpeciesID].AggressionLevel : 0;
            int32 NewAggro = SpeciesList.IsValidIndex(SpeciesID) ? SpeciesList[SpeciesID].AggressionLevel : 0;

            // Если новый полип сильнее, он вытесняет старый
            if (NewAggro > CurrentAggro) {
                Node->SpeciesID = SpeciesID;
            }
        } else {
            Node->bIsOccupied = true;
            Node->SpeciesID = SpeciesID;
        }
        Node->bIsLeaf = true;
        return;
    }

    Node->bIsLeaf = false;
    int32 Half = Size / 2;
    int32 ChildIdx = GetOctantIndex(TargetPos, NodePos, Half);

    if (!Node->Children[ChildIdx]) {
        Node->Children[ChildIdx] = new FVoxelOctreeNode();
    }

    FIntVector ChildNodePos = NodePos;
    if (ChildIdx & 1) ChildNodePos.X += Half;
    if (ChildIdx & 2) ChildNodePos.Y += Half;
    if (ChildIdx & 4) ChildNodePos.Z += Half;

    // Рекурсивный вызов с увеличением глубины
    InsertRecursive(Node->Children[ChildIdx], ChildNodePos, Half, Depth + 1, TargetPos, SpeciesID, SpeciesList);
}

bool FVoxelOctree::IsInside(FIntVector GlobalPos) const {
    return (GlobalPos.X >= TreeOffset.X && GlobalPos.X < TreeOffset.X + CurrentWorldSize) &&
           (GlobalPos.Y >= TreeOffset.Y && GlobalPos.Y < TreeOffset.Y + CurrentWorldSize) &&
           (GlobalPos.Z >= TreeOffset.Z && GlobalPos.Z < TreeOffset.Z + CurrentWorldSize);
}

int32 FVoxelOctree::GetOctantIndex(const FIntVector& Target, const FIntVector& MinBound, int32 HalfSize) const {
    int32 Index = 0;
    if (Target.X >= MinBound.X + HalfSize) Index |= 1;
    if (Target.Y >= MinBound.Y + HalfSize) Index |= 2;
    if (Target.Z >= MinBound.Z + HalfSize) Index |= 4;
    return Index;
}

int32 FVoxelOctree::GetOldRootIndexInNewRoot(const FIntVector& OldOffset, const FIntVector& NewOffset) const {
    int32 Index = 0;
    if (OldOffset.X > NewOffset.X) Index |= 1;
    if (OldOffset.Y > NewOffset.Y) Index |= 2;
    if (OldOffset.Z > NewOffset.Z) Index |= 4;
    return Index;
}