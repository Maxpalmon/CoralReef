// Fill out your copyright notice in the Description page of Project Settings.


#include "FVoxelOctree.h"

FVoxelOctree::FVoxelOctree(int32 InWorldSize):WorldSize(InWorldSize)
{
	MaxDepth = FMath::CeilLogTwo(MaxDepth);
	Root = new FVoxelOctreeNode();
}

FVoxelOctree::~FVoxelOctree()
{
	delete Root;
}

int32 FVoxelOctree::GetSpeciesAt(FIntVector Pos) const 
{
	if (!Root) return -1;

	FVoxelOctreeNode* Current = Root;
	FIntVector NodePos(0);
	int32 Size = WorldSize;

	// Спускаемся по дереву до максимальной глубины
	for (int32 Depth = 0; Depth < MaxDepth; ++Depth)
	{
		int32 Half = Size / 2;
		int32 ChildIdx = GetOctantIndex(Pos, NodePos, Half);

		// Если ветки не существует — вокселя там точно нет
		if (!Current->Children[ChildIdx]) 
		{
			return -1;
		}

		// Переходим глубже
		Current = Current->Children[ChildIdx];
        
		// Обновляем позицию текущего куба для следующего шага
		if (ChildIdx & 1) NodePos.X += Half;
		if (ChildIdx & 2) NodePos.Y += Half;
		if (ChildIdx & 4) NodePos.Z += Half;
		Size = Half;
	}

	// Мы дошли до "листа" (вокселя). Возвращаем его владельца.
	return Current->bIsOccupied ? Current->SpeciesID : -1;
}

bool FVoxelOctree::IsOccupied(FIntVector Pos) const 
{
	// Если поиск вернул любой ID, кроме -1, значит там воксель
	return GetSpeciesAt(Pos) != -1;
}

int32 FVoxelOctree::GetOctantIndex(const FIntVector& Target, const FIntVector& NodePos, int32 HalfSize) const {
	int32 Index = 0;
	if (Target.X >= NodePos.X + HalfSize) Index |= 1;
	if (Target.Y >= NodePos.Y + HalfSize) Index |= 2;
	if (Target.Z >= NodePos.Z + HalfSize) Index |= 4;
	return Index;
}

void FVoxelOctree::SetVoxel(FIntVector Pos, int32 SpeciesID) {
	InsertRecursive(Root, FIntVector(0), WorldSize, 0, Pos, SpeciesID);
}

void FVoxelOctree::InsertRecursive(FVoxelOctreeNode* Node, FIntVector NodePos, int32 Size, int32 Depth, FIntVector TargetPos, int32 SpeciesID) {
	if (Depth == MaxDepth) {
		// Логика конкуренции: если занято чужим видом, можно "перекрасить" (захватить)
		Node->bIsOccupied = true;
		Node->SpeciesID = SpeciesID;
		return;
	}

	int32 Half = Size / 2;
	int32 ChildIdx = GetOctantIndex(TargetPos, NodePos, Half);

	if (!Node->Children[ChildIdx]) {
		Node->Children[ChildIdx] = new FVoxelOctreeNode();
	}

	FIntVector ChildOffset(
		(ChildIdx & 1) ? Half : 0,
		(ChildIdx & 2) ? Half : 0,
		(ChildIdx & 4) ? Half : 0
	);

	InsertRecursive(Node->Children[ChildIdx], NodePos + ChildOffset, Half, Depth + 1, TargetPos, SpeciesID);
}