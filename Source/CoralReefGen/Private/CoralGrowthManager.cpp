#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

ACoralGrowthManager::ACoralGrowthManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // Инициализируем основной компонент для мешей
    VoxelMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = VoxelMeshComponent;

    // Настраиваем поддержку Custom Data (0:R, 1:G, 2:B) для шейдера
    VoxelMeshComponent->NumCustomDataFloats = 3;
    
    // Привязываем MeshComponent к тому же объекту для обратной совместимости
    MeshComponent = VoxelMeshComponent;

    VoxelSize = 100.0f;
    TotalVoxelCount = 0;
    CurrentStep = 0;
    bIsAutoGrowth = false;
}

void ACoralGrowthManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Спавним первый вид (Index 0) в центре
    SpawnSeed(FIntVector(0, 0, 0), 0);

    // Спавним второй вид (Index 1) чуть в стороне, чтобы они столкнулись
    // Если размер вокселя 100, то 20 вокселей = 2000 юнитов
    SpawnSeed(FIntVector(20, 0, 0), 1);
    
    VoxelStorage = NewObject<UVoxelStorage>(this);
    
    // Инициализируем пресеты видов
    InitializeSpecies();
    
    // Создаем SVO дерево (начальный размер 1024, расширяется автоматически)
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

    // Регистрируем в октодереве (передаем SpeciesList для будущей борьбы)
    GlobalOctree->SetVoxel(Pos, ColonyID, SpeciesList);
    VoxelStorage->SetVoxel(Pos, FVoxelData(EVoxelType::Living, ColonyID));

    // Визуализируем кубик нужного цвета
    SpawnVoxelVisual(Pos, ColonyID);

    // Запоминаем индекс только что созданного инстанса
    int32 NewIdx = VoxelMeshComponent->GetInstanceCount() - 1;

    // Добавляем агента в активный список
    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID, NewIdx));

    TotalVoxelCount++;
    LivingPolypCount = ActiveAgents.Num();
}

void ACoralGrowthManager::SimulationStep()
{
    if (!VoxelStorage || !GlobalOctree || ActiveAgents.Num() == 0) return;

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    // 1. Статистика и превращение текущих агентов в скелет (визуальное старение)
    SkeletonPolypCount += ActiveAgents.Num();
    for (const FCoralAgent& OldAgent : ActiveAgents)
    {
        if (OldAgent.InstanceIndex != -1)
        {
            // Перекрашиваем инстанс в цвет скелета
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 0, SkeletonColor.R);
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 1, SkeletonColor.G);
            VoxelMeshComponent->SetCustomDataValue(OldAgent.InstanceIndex, 2, SkeletonColor.B);
        }
    }

    // 2. Основной цикл симуляции роста для каждого активного полипа
    for (const FCoralAgent& Agent : ActiveAgents)
    {
        // Проверка плотности (Density Check) - если слишком тесно, агент умирает
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

        TArray<FIntVector> ValidDirs;
        for (const FIntVector& Dir : Directions)
        {
            FIntVector NeighborPos = Agent.Position + Dir;

            // --- ПРОВЕРКА ЧЕРЕЗ SVO (МЕЖВИДОВАЯ БОРЬБА) ---
            if (GlobalOctree->IsOccupied(NeighborPos))
            {
                int32 OtherSpecies = GlobalOctree->GetSpeciesAt(NeighborPos);
                if (OtherSpecies != Agent.ColonyID)
                {
                    // Сравниваем AggressionLevel из пресетов
                    if (SpeciesList.IsValidIndex(Agent.ColonyID) && SpeciesList.IsValidIndex(OtherSpecies))
                    {
                        // Если наш уровень агрессии ниже или равен — не растем в эту клетку
                        if (SpeciesList[Agent.ColonyID].AggressionLevel <= SpeciesList[OtherSpecies].AggressionLevel)
                        {
                            continue;
                        }
                    }
                }
                else continue; // Это наш собственный вид, место занято
            }

            // Проверка через локальное хранилище чанков
            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;

            // Проверка кучности (MaxDensity)
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

        // Сортировка направлений по весам (Фототропизм, Инерция, Поток)
        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            float ScoreA = (A == Agent.Direction ? InertiaWeight : 0.0f) + (A.Z > 0 ? PhototropismWeight : 0.0f);
            float ScoreB = (B == Agent.Direction ? InertiaWeight : 0.0f) + (B.Z > 0 ? PhototropismWeight : 0.0f);
            
            FVector FlowNorm = CurrentDirection.GetSafeNormal();
            ScoreA += FVector::DotProduct(FVector(A).GetSafeNormal(), FlowNorm) * CurrentStrength;
            ScoreB += FVector::DotProduct(FVector(B).GetSafeNormal(), FlowNorm) * CurrentStrength;

            return ScoreA > ScoreB;
        });

        // Процесс роста
        float Chance = (TotalVoxelCount < 15) ? 1.0f : GrowthProbability;
        int32 MaxNewBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 BranchesCreated = 0;

        for (const FIntVector& ChosenDir : ValidDirs)
        {
            if (BranchesCreated >= MaxNewBranches) break;

            if (FMath::FRand() < Chance)
            {
                FIntVector NewPos = Agent.Position + ChosenDir;

                // Регистрация в SVO (тут происходит физическая перезапись, если агрессия выше)
                GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList);
                VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                // Создаем визуал
                SpawnVoxelVisual(NewPos, Agent.ColonyID);

                // Получаем индекс нового инстанса для управления цветом в будущем
                int32 NewIdx = VoxelMeshComponent->GetInstanceCount() - 1;
                NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));

                TotalVoxelCount++;
                BranchesCreated++;
            }
        }
    }

    ActiveAgents = NextGeneration;
    LivingPolypCount = ActiveAgents.Num();

    ExportStatsToCSV();
}

void ACoralGrowthManager::SpawnVoxelVisual(FIntVector Pos, int32 SpeciesID)
{
    if (!VoxelMeshComponent || !SpeciesList.IsValidIndex(SpeciesID)) return;

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(FVector(Pos) * VoxelSize);
    SpawnTransform.SetScale3D(FVector(VoxelSize / 100.0f)); 

    int32 InstanceIdx = VoxelMeshComponent->AddInstance(SpawnTransform);

    // Устанавливаем цвет из SpeciesList
    FColor Col = SpeciesList[SpeciesID].DisplayColor;

    // Передаем данные в Custom Data (индексы 0, 1, 2 соответствуют R, G, B)
    VoxelMeshComponent->SetCustomDataValue(InstanceIdx, 0, Col.R / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(InstanceIdx, 1, Col.G / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(InstanceIdx, 2, Col.B / 255.0f);
}

void ACoralGrowthManager::InitializeSpecies()
{
    // Инициализируем только если список пуст (чтобы не затирать настройки из Blueprint)
    if (SpeciesList.Num() == 0)
    {
        // 1. Типичный ветвистый коралл
        FSpeciesParams Acropora;
        Acropora.Name = "Acropora";
        Acropora.DisplayColor = FColor::Cyan;
        Acropora.AggressionLevel = 1;
        SpeciesList.Add(Acropora);

        // 2. Массивный мозговой коралл
        FSpeciesParams Brain;
        Brain.Name = "Brain Coral";
        Brain.DisplayColor = FColor::Orange;
        Brain.AggressionLevel = 5; // Более агрессивный
        SpeciesList.Add(Brain);
    }
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