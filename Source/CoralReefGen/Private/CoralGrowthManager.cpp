#include "CoralGrowthManager.h"

ACoralGrowthManager::ACoralGrowthManager()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = MeshComponent;
}

void ACoralGrowthManager::BeginPlay()
{
    Super::BeginPlay();
    VoxelStorage = NewObject<UVoxelStorage>(this);
}

void ACoralGrowthManager::SpawnSeed(FIntVector Pos, int32 ColonyID)
{
    if (!VoxelStorage) return;

    VoxelStorage->SetVoxel(Pos, FVoxelData(EVoxelType::Living, ColonyID));
    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID));

    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    MeshComponent->AddInstance(T);

    TotalVoxelCount = 1;
    CurrentStep = 0;
}

void ACoralGrowthManager::SimulationStep()
{
    if (!VoxelStorage || ActiveAgents.Num() == 0) return;

    CurrentStep++; // Увеличиваем шаг
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    for (const FCoralAgent& Agent : ActiveAgents)
    {
        TArray<FIntVector> ValidDirs;
        for (const FIntVector& Dir : Directions)
        {
            FIntVector NeighborPos = Agent.Position + Dir;
            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;

            int32 Contacts = 0;
            for (const FIntVector& CheckDir : Directions)
            {
                if (VoxelStorage->GetVoxel(NeighborPos + CheckDir).Type != EVoxelType::Empty)
                    Contacts++;
            }
            if (Contacts > MaxDensity) continue;
            ValidDirs.Add(Dir);
        }

        if (ValidDirs.Num() == 0) continue;

        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            float ScoreA = (A == Agent.Direction ? InertiaWeight : 0.0f) + (A.Z > 0 ? PhototropismWeight : 0.0f);
            float ScoreB = (B == Agent.Direction ? InertiaWeight : 0.0f) + (B.Z > 0 ? PhototropismWeight : 0.0f);
            return ScoreA > ScoreB;
            });

        int32 MaxNewBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 BranchesCreated = 0;

        for (const FIntVector& ChosenDir : ValidDirs)
        {
            if (BranchesCreated >= MaxNewBranches) break;
            // --- ГАРАНТИРОВАННЫЙ СТАРТ ---
 // Если коралл еще совсем маленький (меньше 15 вокселей), 
 // он растет со 100% шансом, чтобы не погибнуть в начале.
            float EffectiveProbability = GrowthProbability;
            if (TotalVoxelCount < 15)
            {
                EffectiveProbability = 1.0f;
            }

            // Теперь используем EffectiveProbability вместо обычной
            if (FMath::FRand() < EffectiveProbability)
            {
                FIntVector NewPos = Agent.Position + ChosenDir;
                VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));
                NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID));

                FTransform T;
                T.SetLocation(FVector(NewPos) * VoxelSize);
                MeshComponent->AddInstance(T);

                TotalVoxelCount++;
                BranchesCreated++;
            }
        }
    }
    ActiveAgents = NextGeneration;
}

FString ACoralGrowthManager::GetSimulationStats()
{
    return FString::Printf(TEXT("Step: %d\nActive polyps: %d\nVoxel total count: %d"),
        CurrentStep, ActiveAgents.Num(), TotalVoxelCount);
}