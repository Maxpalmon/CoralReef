#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ACoralGrowthManager::ACoralGrowthManager()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = MeshComponent;

    // Включаем поддержку Custom Data (0-R, 1-G, 2-B)
    MeshComponent->NumCustomDataFloats = 3;
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

    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    int32 NewIdx = MeshComponent->AddInstance(T);

    // Добавляем первого агента
    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID, NewIdx));

    // Красим в живой цвет
    MeshComponent->SetCustomDataValue(NewIdx, 0, LivingColor.R);
    MeshComponent->SetCustomDataValue(NewIdx, 1, LivingColor.G);
    MeshComponent->SetCustomDataValue(NewIdx, 2, LivingColor.B);

    TotalVoxelCount = 1;
    LivingPolypCount = 1;
    SkeletonVoxelCount = 0;
    CurrentStep = 0;
}

void ACoralGrowthManager::ToggleAutoGrowth(bool bEnable)
{
    bIsAutoGrowth = bEnable;
    if (bIsAutoGrowth)
    {
        GetWorldTimerManager().SetTimer(GrowthTimerHandle, this, &ACoralGrowthManager::SimulationStep, StepDelay, true);
    }
    else
    {
        GetWorldTimerManager().ClearTimer(GrowthTimerHandle);
    }
}

void ACoralGrowthManager::SimulationStep()
{
    if (!VoxelStorage || ActiveAgents.Num() == 0) return;

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    // 1. Статистика и перекраска активных агентов в цвет скелета
    SkeletonVoxelCount += ActiveAgents.Num();
    for (const FCoralAgent& OldAgent : ActiveAgents)
    {
        if (OldAgent.InstanceIndex != -1)
        {
            MeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 0, SkeletonColor.R);
            MeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 1, SkeletonColor.G);
            MeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 2, SkeletonColor.B);
        }
    }

    // 2. Основной цикл симуляции
    for (const FCoralAgent& Agent : ActiveAgents)
    {
        // --- РАДИАЛЬНАЯ ОПТИМИЗАЦИЯ ПЛОТНОСТИ ---
        // Проверяем заполненность пространства вокруг (куб 3x3x3)
        int32 DensityCheck = 0;
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    if (x == 0 && y == 0 && z == 0) continue;
                    if (VoxelStorage->GetVoxel(Agent.Position + FIntVector(x, y, z)).Type != EVoxelType::Empty)
                        DensityCheck++;
                }
            }
        }
        // Если вокруг слишком тесно (порог 12 из 26), полип "задыхается" и перестает расти
        if (DensityCheck >= 12) continue;

        // --- ПРОВЕРКА НА ЗАТЕНЕНИЕ (СВЕТ) ---
        int32 OverheadObstacles = 0;
        for (int32 z = 1; z <= 2; z++) {
            if (VoxelStorage->GetVoxel(Agent.Position + FIntVector(0, 0, z)).Type != EVoxelType::Empty)
                OverheadObstacles++;
        }
        if (OverheadObstacles >= LightThreshold) continue;

        // --- ПОИСК ДОСТУПНЫХ НАПРАВЛЕНИЙ ---
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

        // --- СОРТИРОВКА (Инерция + Фототропизм + Течение) ---
        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            FVector DirA = FVector(A).GetSafeNormal();
            FVector DirB = FVector(B).GetSafeNormal();
            FVector FlowNorm = CurrentDirection.GetSafeNormal();

            float ScoreA = (A == Agent.Direction ? InertiaWeight : 0.0f) + (A.Z > 0 ? PhototropismWeight : 0.0f);
            float ScoreB = (B == Agent.Direction ? InertiaWeight : 0.0f) + (B.Z > 0 ? PhototropismWeight : 0.0f);

            ScoreA += FVector::DotProduct(DirA, FlowNorm) * CurrentStrength;
            ScoreB += FVector::DotProduct(DirB, FlowNorm) * CurrentStrength;

            return ScoreA > ScoreB;
            });

        // --- РОСТ ---
        float Chance = (TotalVoxelCount < 15) ? 1.0f : GrowthProbability;
        int32 MaxNewBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 BranchesCreated = 0;

        for (const FIntVector& ChosenDir : ValidDirs)
        {
            if (BranchesCreated >= MaxNewBranches) break;

            if (FMath::FRand() < Chance)
            {
                FIntVector NewPos = Agent.Position + ChosenDir;
                VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                FTransform T;
                T.SetLocation(FVector(NewPos) * VoxelSize);
                int32 NewIdx = MeshComponent->AddInstance(T);

                NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));

                // Красим новый полип в живой цвет
                MeshComponent->SetCustomDataValue(NewIdx, 0, LivingColor.R);
                MeshComponent->SetCustomDataValue(NewIdx, 1, LivingColor.G);
                MeshComponent->SetCustomDataValue(NewIdx, 2, LivingColor.B);

                TotalVoxelCount++;
                BranchesCreated++;
            }
        }
    }

    ActiveAgents = NextGeneration;
    LivingPolypCount = ActiveAgents.Num();

    // Авто-экспорт в CSV каждый шаг для удобства сбора данных
    ExportStatsToCSV();
}

void ACoralGrowthManager::ExportStatsToCSV()
{
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("CoralStats.csv");

    if (!FPaths::FileExists(FilePath)) {
        FFileHelper::SaveStringToFile(TEXT("Step,Living,Skeleton,Total\n"), *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    }

    FString DataRow = FString::Printf(TEXT("%d,%d,%d,%d\n"), CurrentStep, LivingPolypCount, SkeletonVoxelCount, TotalVoxelCount);
    FFileHelper::SaveStringToFile(DataRow, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

FString ACoralGrowthManager::GetSimulationStats()
{
    return FString::Printf(TEXT("Step: %d\nLive polyps: %d\nScelet volume: %d\nTotal voxels count: %d"),
        CurrentStep, LivingPolypCount, SkeletonVoxelCount, TotalVoxelCount);
}