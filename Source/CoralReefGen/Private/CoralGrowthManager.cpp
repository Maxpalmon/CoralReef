#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


ACoralGrowthManager::ACoralGrowthManager()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = MeshComponent;

    // Поддержка Custom Data для шейдера (0-R, 1-G, 2-B)
    MeshComponent->NumCustomDataFloats = 3;
    
    // Инициализация стандартных значений
    VoxelSize = 100.0f;
    TotalVoxelCount = 0;
    CurrentStep = 0;
    bIsAutoGrowth = false;
}

void ACoralGrowthManager::BeginPlay()
{
    Super::BeginPlay();
    VoxelStorage = NewObject<UVoxelStorage>(this);
    
    // Инициализация экспериментального октодерева (размер 1024 вокселя)
    GlobalOctree = new FVoxelOctree(1024);
}

void ACoralGrowthManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GlobalOctree)
    {
        delete GlobalOctree;
        GlobalOctree = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void ACoralGrowthManager::SpawnSeed(FIntVector Pos, int32 ColonyID)
{
    if (!VoxelStorage || !GlobalOctree) return;

    // Регистрация в обеих системах
    VoxelStorage->SetVoxel(Pos, FVoxelData(EVoxelType::Living, ColonyID));
    GlobalOctree->SetVoxel(Pos, ColonyID);

    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    int32 NewIdx = MeshComponent->AddInstance(T);

    // Добавляем первого агента
    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID, NewIdx));

    // Красим в цвет живого полипа
    MeshComponent->SetCustomDataValue(NewIdx, 0, LivingColor.R);
    MeshComponent->SetCustomDataValue(NewIdx, 1, LivingColor.G);
    MeshComponent->SetCustomDataValue(NewIdx, 2, LivingColor.B);

    TotalVoxelCount++;
    LivingPolypCount = 1;
}

void ACoralGrowthManager::SimulationStep()
{
    if (!VoxelStorage || !GlobalOctree || ActiveAgents.Num() == 0) return;

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    // 1. Статистика и перекрашивание текущих агентов в цвет скелета
    SkeletonPolypCount += ActiveAgents.Num();
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
        // Оптимизация плотности (Density Check 3x3x3)
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
        if (DensityCheck >= 12) continue;

        // Поиск валидных направлений для роста
        TArray<FIntVector> ValidDirs;
        for (const FIntVector& Dir : Directions)
        {
            FIntVector NeighborPos = Agent.Position + Dir;

            UE_LOG(LogTemp, Warning, TEXT("Checking Pos: %s | Occupied: %s"), *NeighborPos.ToString(), GlobalOctree->IsOccupied(NeighborPos) ? TEXT("True") : TEXT("False"));
            // --- ПРОВЕРКА КОНКУРЕНЦИИ ЧЕРЕЗ SVO ---
            if (GlobalOctree->IsOccupied(NeighborPos))
            {
                int32 OtherSpecies = GlobalOctree->GetSpeciesAt(NeighborPos);
                if (OtherSpecies != Agent.ColonyID)
                {
                    // Если наш вид агрессивнее, мы блокируем рост соседа (или можем его поглотить)
                    if (SpeciesList.IsValidIndex(Agent.ColonyID) && SpeciesList.IsValidIndex(OtherSpecies))
                    {
                        if (SpeciesList[Agent.ColonyID].AggressionLevel > SpeciesList[OtherSpecies].AggressionLevel)
                        {
                            // Логика подавления
                        }
                    }
                }
                continue; // Клетка занята, расти сюда нельзя
            }

            // Проверка через VoxelStorage (для подстраховки)
            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;

            // Проверка кучности (Contacts)
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

        // Сортировка по весам (Инерция + Фототропизм + Поток)
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

        // Процесс деления (Growth)
        float Chance = (TotalVoxelCount < 15) ? 1.0f : GrowthProbability;
        int32 MaxNewBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 BranchesCreated = 0;

        for (const FIntVector& ChosenDir : ValidDirs)
        {
            if (BranchesCreated >= MaxNewBranches) break;

            if (FMath::FRand() < Chance)
            {
                FIntVector NewPos = Agent.Position + ChosenDir;

                // Регистрация в двух структурах (Чанки и SVO)
                VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));
                GlobalOctree->SetVoxel(NewPos, Agent.ColonyID);

                // Визуализация в UE
                FTransform T;
                T.SetLocation(FVector(NewPos) * VoxelSize);
                int32 NewIdx = MeshComponent->AddInstance(T);

                NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));

                // Установка цвета живого полипа
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

    ExportStatsToCSV();
}

void ACoralGrowthManager::ExportStatsToCSV()
{
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("CoralStats.csv");

    if (!FPaths::FileExists(FilePath)) {
        FFileHelper::SaveStringToFile(TEXT("Step,Living,Skeleton,Total\n"), *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    }

    FString DataRow = FString::Printf(TEXT("%d,%d,%d,%d\n"), CurrentStep, LivingPolypCount, SkeletonPolypCount, TotalVoxelCount);
    FFileHelper::SaveStringToFile(DataRow, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
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

FString ACoralGrowthManager::GetSimulationStats()
{
    return FString::Printf(TEXT("Step: %d\nLive polyps: %d\nSkeleton volume: %d\nTotal voxels count: %d"),
        CurrentStep, LivingPolypCount, SkeletonPolypCount, TotalVoxelCount);
}