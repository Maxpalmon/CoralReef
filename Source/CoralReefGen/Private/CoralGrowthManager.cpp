#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

ACoralGrowthManager::ACoralGrowthManager() {
    PrimaryActorTick.bCanEverTick = false;
    VoxelMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = VoxelMeshComponent;
    VoxelMeshComponent->NumCustomDataFloats = 3;
    MeshComponent = VoxelMeshComponent; // Синхронизация указателей

    VoxelSize = 100.0f;
    TotalVoxelCount = 0;
    CurrentStep = 0;
}

void ACoralGrowthManager::BeginPlay() {
    Super::BeginPlay();
    VoxelStorage = NewObject<UVoxelStorage>(this);
    InitializeSpecies();
    GlobalOctree = new FVoxelOctree(1024);
}

void ACoralGrowthManager::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    if (GlobalOctree) { delete GlobalOctree; GlobalOctree = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void ACoralGrowthManager::SpawnSeed(FIntVector Pos, int32 ColonyID) {
    if (!VoxelStorage || !GlobalOctree) return;

    // 1. Создаем визуал и получаем индекс
    int32 NewIdx = SpawnVoxelVisualWithReturn(Pos, ColonyID);

    // 2. Регистрируем в SVO с индексом
    GlobalOctree->SetVoxel(Pos, ColonyID, SpeciesList, VoxelMeshComponent, NewIdx);
    VoxelStorage->SetVoxel(Pos, FVoxelData(EVoxelType::Living, ColonyID));

    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID, NewIdx));
    TotalVoxelCount++;
}

void ACoralGrowthManager::SimulationStep() {
    if (!VoxelStorage || !GlobalOctree || ActiveAgents.Num() == 0) return;

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    // 1. Старение полипов: берем цвет их собственного вида, но делаем чуть темнее
    for (const FCoralAgent& OldAgent : ActiveAgents) {
        if (OldAgent.InstanceIndex != -1 && SpeciesList.IsValidIndex(OldAgent.ColonyID)) {
            FColor SpeciesCol = SpeciesList[OldAgent.ColonyID].DisplayColor;
            float Dim = 0.7f; // Скелет чуть тусклее живого края
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 0, (SpeciesCol.R / 255.0f) * Dim);
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 1, (SpeciesCol.G / 255.0f) * Dim);
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 2, (SpeciesCol.B / 255.0f) * Dim);
        }
    }

    // 2. Рост
    for (const FCoralAgent& Agent : ActiveAgents) {
        TArray<FIntVector> ValidDirs;
        for (const FIntVector& Dir : Directions) {
            FIntVector NeighborPos = Agent.Position + Dir;

            // Проверка SVO: если занято кем-то другим, проверяем агрессию
            if (GlobalOctree->IsOccupied(NeighborPos)) {
                int32 OtherSpecies = GlobalOctree->GetSpeciesAt(NeighborPos);
                if (OtherSpecies != Agent.ColonyID) {
                    if (SpeciesList.IsValidIndex(Agent.ColonyID) && SpeciesList.IsValidIndex(OtherSpecies)) {
                        if (SpeciesList[Agent.ColonyID].AggressionLevel <= SpeciesList[OtherSpecies].AggressionLevel)
                            continue; // Враг сильнее, не лезем
                    }
                } else continue;
            }

            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;
            ValidDirs.Add(Dir);
        }

        if (ValidDirs.Num() == 0) continue;

        // Сортировка направлений (Свет + Поток)
        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            float ScoreA = (A.Z > 0 ? PhototropismWeight : 0.0f);
            float ScoreB = (B.Z > 0 ? PhototropismWeight : 0.0f);
            return ScoreA > ScoreB;
        });

        int32 MaxBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 Created = 0;

        for (const FIntVector& ChosenDir : ValidDirs) {
            if (Created >= MaxBranches) break;
            if (FMath::FRand() < GrowthProbability) {
                FIntVector NewPos = Agent.Position + ChosenDir;

                // ВАЖНО: Сначала визуал, потом SVO
                int32 NewIdx = SpawnVoxelVisualWithReturn(NewPos, Agent.ColonyID);
                GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList, VoxelMeshComponent, NewIdx);
                VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));
                TotalVoxelCount++;
                Created++;
            }
        }
    }
    ActiveAgents = NextGeneration;
    ExportStatsToCSV();
}

int32 ACoralGrowthManager::SpawnVoxelVisualWithReturn(FIntVector Pos, int32 SpeciesID) {
    if (!VoxelMeshComponent || !SpeciesList.IsValidIndex(SpeciesID)) return -1;

    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    int32 Idx = VoxelMeshComponent->AddInstance(T);
    
    FColor Col = SpeciesList[SpeciesID].DisplayColor;
    VoxelMeshComponent->SetCustomDataValue(Idx, 0, Col.R / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(Idx, 1, Col.G / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(Idx, 2, Col.B / 255.0f);
    
    return Idx;
}

void ACoralGrowthManager::InitializeSpecies() {
    if (SpeciesList.Num() == 0) {
        FSpeciesParams S1; S1.Name = "Cyan"; S1.DisplayColor = FColor::Cyan; S1.AggressionLevel = 1;
        SpeciesList.Add(S1);
        FSpeciesParams S2; S2.Name = "Red"; S2.DisplayColor = FColor::Red; S2.AggressionLevel = 10;
        SpeciesList.Add(S2);
    }
}

void ACoralGrowthManager::ExportStatsToCSV() { /* ... код из предыдущих ответов ... */ }
void ACoralGrowthManager::ToggleAutoGrowth(bool bEnable) { /* ... код из предыдущих ответов ... */ }