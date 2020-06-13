#include "MyUObjectTestClsFactory.h"
#include "IM4UPrivatePCH.h"
#include "MyUObjectTestCls.h"

#include "CoreMinimal.h"
#include "ComponentReregisterContext.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Subsystems/ImportSubsystem.h"

#include "PackageTools.h"
#include "RawMesh.h"

#include "PmxImporter.h"
#include "PmdImporter.h"


#include "ObjectTools.h"

#include "EncodeHelper.h"

DEFINE_LOG_CATEGORY(LogCategoryPMXFactory)


UMyUObjectTestClsFactory::UMyUObjectTestClsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMyUObjectTestCls::StaticClass();
	bCreateNew = false;
	////////////
	bEditorImport = true;
	bText = false;
	Formats.Add(TEXT("pmd_st;MMD-PMD test static mesh debug"));
	Formats.Add(TEXT("pmx_st;MMD-PMX"));
}

bool UMyUObjectTestClsFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UMyUObjectTestCls::StaticClass());
}

UClass* UMyUObjectTestClsFactory::ResolveSupportedClass()
{
	return UMyUObjectTestCls::StaticClass();
}

UObject* UMyUObjectTestClsFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
	)
{
	return NewObject<UMyUObjectTestCls>(InParent, InClass, InName, Flags);;
}

UObject* UMyUObjectTestClsFactory::FactoryCreateBinary(
	UClass * InClass,
	UObject * InParent,
	FName InName,
	EObjectFlags Flags,
	UObject * Context,
	const TCHAR * Type,
	const uint8 *& Buffer,
	const uint8 * BufferEnd,
	FFeedbackContext * Warn
	)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	check(InClass == UMyUObjectTestCls::StaticClass());

	UMyUObjectTestCls* NewMyAsset = NULL;
	
	NewMyAsset = NewObject<UMyUObjectTestCls>(InParent, InClass, InName, Flags);

	pmxMaterialImport.InitializeBaseValue(InParent);

	bool bIsPmxFormat = false;
	if (FString(Type).Equals(TEXT("pmx_st"), ESearchCase::IgnoreCase))
	{
		//Is PMX format 
		bIsPmxFormat = true;
	}
	if (bIsPmxFormat == false)
	{
		//PMD
		MMD4UE4::PmdMeshInfo PmdMeshInfo;
		bool isPMDBinaryLoad = PmdMeshInfo.PMDLoaderBinary(Buffer, BufferEnd);

		MMD4UE4::PmxMeshInfo PmxMeshInfo;
		PmdMeshInfo.ConvertToPmxFormat(&PmxMeshInfo);
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMD Import Header Complete."));
		///////////
		//PMX
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.Broadcast(this, NewMyAsset);
		int NodeIndex = 0;
		int NodeIndexMax = 0;
		//MMD4UE4::PmxMeshInfo pmxMeshInfoPtr;
		bool isPMXBinaryLoad = PmxMeshInfo.PMXLoaderBinary(Buffer, BufferEnd);
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Header Complete."));
		////////////////////////////////////////////
		NodeIndexMax += PmxMeshInfo.textureList.Num();
		NodeIndexMax += PmxMeshInfo.materialList.Num();
		NodeIndexMax += 1;//static mesh
		//AssetsCreateTextuer(InParent, Flags, Warn, L"nAHN_TargetMarker.png");
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import TEST Complete."));
		///////////////////////////////////////////
		TArray<UTexture*> textureAssetList;
		for (int k = 0; k < PmxMeshInfo.textureList.Num(); ++k)
		{
			pmxMaterialImport.AssetsCreateTextuer(
				//InParent,
				//Flags,
				//Warn,
				FPaths::GetPath(GetCurrentFilename()),
				PmxMeshInfo.textureList[k].TexturePath,
				textureAssetList
				);

			//if (NewObject)
			{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
			}
		}
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Texture Extecd Complete."));
		////////////////////////////////////////////

		TArray<UMaterialInterface*> OutMaterials;
		TArray<FString> UVSets;
		for (int k = 0; k < PmxMeshInfo.materialList.Num(); ++k)
		{
			pmxMaterialImport.CreateUnrealMaterial(
				PmxMeshInfo.modelNameJP,
				//InParent,
				PmxMeshInfo.materialList[k],
				true,
				false,
				OutMaterials,
				textureAssetList);
			//if (NewObject)
			{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
			}
		}
		///////////////////////////////////////////
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Material Extecd Complete."));
		///////////////////////////////////////////

		FString InStaticMeshPackageName
			= "";
			// = ("/Game/SM_");
		InStaticMeshPackageName += PmxMeshInfo.modelNameJP;
		FVector InPivotLocation;
		ConvertBrushesToStaticMesh(
			InParent,
			InStaticMeshPackageName,
			//TArray<ABrush*>& InBrushesToConvert, 
			&PmxMeshInfo,
			InPivotLocation,
			OutMaterials
			);
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Static Mesh Complete."));


		NodeIndex++;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeIndex"), NodeIndex);
		Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
		GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
		
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import ALL Complete.[%s]"), *(NewMyAsset->MyStruct.ModelName));
		//////////////////////////////////////////////////
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMD Import ALL Complete.[FileName--PMD]"));
	}
	else
	{
		//PMX
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.Broadcast(this, NewMyAsset);

		int NodeIndex = 0;
		int NodeIndexMax = 0;
		MMD4UE4::PmxMeshInfo pmxMeshInfoPtr;
		bool isPMXBinaryLoad = pmxMeshInfoPtr.PMXLoaderBinary(Buffer, BufferEnd);
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Header Complete."));
		////////////////////////////////////////////
		NodeIndexMax += pmxMeshInfoPtr.textureList.Num();
		NodeIndexMax += pmxMeshInfoPtr.materialList.Num();
		NodeIndexMax += 1;//static mesh
		//AssetsCreateTextuer(InParent, Flags, Warn, L"nAHN_TargetMarker.png");
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import TEST Complete."));
		///////////////////////////////////////////
		TArray<UTexture*> textureAssetList;
		for (int k = 0; k < pmxMeshInfoPtr.textureList.Num(); ++k)
		{
			pmxMaterialImport.AssetsCreateTextuer(
				//InParent,
				//Flags,
				//Warn,
				FPaths::GetPath(GetCurrentFilename()),
				pmxMeshInfoPtr.textureList[k].TexturePath,
				textureAssetList
				);

			//if (NewObject)
			{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
			}
		}
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Texture Extecd Complete."));
		////////////////////////////////////////////
		
		TArray<UMaterialInterface*> OutMaterials; 
		TArray<FString> UVSets;
		for (int k = 0; k < pmxMeshInfoPtr.materialList.Num(); ++k)
		{
			pmxMaterialImport.CreateUnrealMaterial(
				pmxMeshInfoPtr.modelNameJP,
				//InParent,
				pmxMeshInfoPtr.materialList[k],
				true,
				false,
				OutMaterials,
				textureAssetList);
			//if (NewObject)
			{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
			}
		}
		///////////////////////////////////////////
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Material Extecd Complete."));
		///////////////////////////////////////////

		FString InStaticMeshPackageName	= "";
		InStaticMeshPackageName += pmxMeshInfoPtr.modelNameJP;
		FVector InPivotLocation;
		ConvertBrushesToStaticMesh(
			InParent,
			InStaticMeshPackageName,
			//TArray<ABrush*>& InBrushesToConvert, 
			&pmxMeshInfoPtr,
			InPivotLocation,
			OutMaterials
			);
		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Static Mesh Complete."));

			NodeIndex++;
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeIndex"), NodeIndex);
			Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
			GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));

		UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import ALL Complete.[%s]"), *(NewMyAsset->MyStruct.ModelName));
	}

	return NewMyAsset;
}

///////////////////////////////////////////////////////////////////////////////////////////
//
//	CreateStaticMeshFromBrush - Creates a static mesh from the triangles in a model.
//

UStaticMesh* UMyUObjectTestClsFactory::CreateStaticMeshFromBrush(
	UObject* Outer,
	FName Name,
	MMD4UE4::PmxMeshInfo *pmxMeshInfoPtr,
	TArray<UMaterialInterface*>& Materials
	//ABrush* Brush,
	//UModel* Model
	)
{
	FRawMesh RawMesh;
	
	GetBrushMesh(/*Brush, Model,*/pmxMeshInfoPtr, RawMesh, Materials);

	UStaticMesh* StaticMesh = CreateStaticMesh(RawMesh, Materials, Outer, Name);

	return StaticMesh;
}

//
//	FVerticesEqual
//

inline bool UMyUObjectTestClsFactory::FVerticesEqual(
	FVector& V1,
	FVector& V2
	)
{
	if (FMath::Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if (FMath::Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if (FMath::Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	return 1;
}

void UMyUObjectTestClsFactory::GetBrushMesh(
	//ABrush* Brush,
	//UModel* Model,
	MMD4UE4::PmxMeshInfo *pmxMeshInfoPtr,
	struct FRawMesh& OutMesh,
	TArray<UMaterialInterface*>& OutMaterials
	)
{
	TArray<FPoly> Polys;
	// Calculate the local to world transform for the source brush.

	FMatrix	ActorToWorld = /*Brush ? Brush->ActorToWorld().ToMatrixWithScale() : */FMatrix::Identity;
	bool	ReverseVertices = true;//For MMD
	FVector4	PostSub = /*Brush ? FVector4(Brush->GetActorLocation()) :*/ FVector4(0, 0, 0, 0);

	int32 matIndx = 0;
	int32 facecount = 0;
	for (int32 i = 0; i < pmxMeshInfoPtr->faseList.Num(); ++i)
	{
		UMaterialInterface*	Material = 0;// Polygon.Material;
		int32 MaterialIndex = 0;
		if (matIndx < pmxMeshInfoPtr->materialList.Num() && matIndx < OutMaterials.Num())
		{
			Material = OutMaterials[matIndx];
			MaterialIndex = matIndx;
		}

		if (!Material)
		{
			// Find a material index for this polygon.
			MaterialIndex = OutMaterials.AddUnique(Material);
		}

		OutMesh.FaceMaterialIndices.Add(MaterialIndex);
		OutMesh.FaceSmoothingMasks.Add(1 << (i % 32));
		if (!ReverseVertices)
		{
			for (int k = 0; k < 3; ++k)
			{
				//int k = 2;
				OutMesh.WedgeIndices.Add(pmxMeshInfoPtr->faseList[i].VertexIndex[k]);
				OutMesh.WedgeTexCoords[0].Add(pmxMeshInfoPtr->vertexList[pmxMeshInfoPtr->faseList[i].VertexIndex[k]].UV);
				FVector TangentZ = pmxMeshInfoPtr->vertexList[pmxMeshInfoPtr->faseList[i].VertexIndex[k]].Normal;
				OutMesh.WedgeTangentZ.Add(TangentZ.GetSafeNormal());
				facecount++;
			}
		}
		else
		{
			for (int k = 2; k > -1; --k)
			{
				OutMesh.WedgeIndices.Add(pmxMeshInfoPtr->faseList[i].VertexIndex[k]);
				OutMesh.WedgeTexCoords[0].Add(pmxMeshInfoPtr->vertexList[pmxMeshInfoPtr->faseList[i].VertexIndex[k]].UV);
				FVector TangentZ = pmxMeshInfoPtr->vertexList[pmxMeshInfoPtr->faseList[i].VertexIndex[k]].Normal;
				OutMesh.WedgeTangentZ.Add(TangentZ.GetSafeNormal());
				facecount++;
			}
		}
		if (Material)
		{
			if (facecount >= pmxMeshInfoPtr->materialList[matIndx].MaterialFaceNum)
			{
				matIndx++;
				facecount = 0;
			}
		}
	}
	// Merge vertices within a certain distance of each other.
	for (int32 i = 0; i < pmxMeshInfoPtr->vertexList.Num(); ++i)
	{
		FVector Position = pmxMeshInfoPtr->vertexList[i].Position;
		OutMesh.VertexPositions.Add(Position);
	}
}


/**
* Creates a static mesh object from raw triangle data.
*/

UStaticMesh* UMyUObjectTestClsFactory::CreateStaticMesh(
	struct FRawMesh& RawMesh,
	TArray<UMaterialInterface*>& Materials,
	UObject* InOuter,
	FName InName
	)
{
	// Create the UStaticMesh object.
	//FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(FindObject<UStaticMesh>(InOuter, *InName.ToString()));
	auto StaticMesh = NewObject<UStaticMesh>(InOuter, InName, RF_Public | RF_Standalone);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.Broadcast(this, StaticMesh);

	// Add one LOD for the base mesh
	FStaticMeshSourceModel *SrcModel = new (StaticMesh->GetSourceModels()) FStaticMeshSourceModel();
	SrcModel->RawMeshBulkData->SaveRawMesh(RawMesh);
	//StaticMesh->Materials = Materials;

	for (int32 MatNum = 0; MatNum < Materials.Num(); ++MatNum)
	{
		StaticMesh->StaticMaterials.Add(FStaticMaterial());
		StaticMesh->StaticMaterials[MatNum].MaterialInterface = Materials[MatNum];
	}

	int32 NumSections = StaticMesh->StaticMaterials.Num();

	// Set up the SectionInfoMap to enable collision
	for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(0, SectionIdx);
		Info.MaterialIndex = SectionIdx;
		Info.bEnableCollision = true;
		StaticMesh->GetSectionInfoMap().Set(0, SectionIdx, Info);
	}
	// @todo This overrides restored values currently 
	// but we need to be able to import over the existing settings 
	// if the user chose to do so.
	SrcModel->BuildSettings.bRemoveDegenerates = 1;//ImportOptions->bRemoveDegenerates;
	SrcModel->BuildSettings.bRecomputeNormals = 0;// ImportOptions->NormalImportMethod == FBXNIM_ComputeNormals;
	SrcModel->BuildSettings.bRecomputeTangents = 1;// ImportOptions->NormalImportMethod != FBXNIM_ImportNormalsAndTangents;
	
	//Test ? unkown
	SrcModel->BuildSettings.bGenerateLightmapUVs = false;
	SrcModel->BuildSettings.SrcLightmapIndex = 1;
	SrcModel->BuildSettings.DstLightmapIndex = 1;
	
	bool buildFlag = true;
	//buildFlag = false;
	StaticMesh->Build(buildFlag);
	StaticMesh->MarkPackageDirty();
	return StaticMesh;
}
/////////////////////////////////////////////////////////////////
AActor* UMyUObjectTestClsFactory::ConvertBrushesToStaticMesh(
	UObject * InParent,
	const FString& InStaticMeshPackageName, 
	//TArray<ABrush*>& InBrushesToConvert,
	MMD4UE4::PmxMeshInfo *pmxMeshInfoPtr,
	const FVector& InPivotLocation,
	TArray<UMaterialInterface*>& Materials
	)
{
	AActor* NewActor(NULL);
	FString StaticMeshName = "SM_" + InStaticMeshPackageName;
	// Make sure we have a parent
	if (!ensure(InParent))
	{
		return NULL;
	}

	FString BasePackageName
		= FPackageName::GetLongPackagePath(
			InParent->GetOutermost()->GetName()) / StaticMeshName;
	FString StaticMeshPackageName;

	FAssetToolsModule& AssetToolsModule
		= FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(
		BasePackageName, TEXT(""),
		StaticMeshPackageName, StaticMeshName);
	UPackage* Pkg = CreatePackage(NULL, *StaticMeshPackageName);

	FName ObjName = *FPackageName::GetLongPackageAssetName(StaticMeshName);

	check(Pkg != nullptr);

	FVector Location(0.0f, 0.0f, 0.0f);
	FRotator Rotation(0.0f, 0.0f, 0.0f);

	UStaticMesh* NewMesh 
		= CreateStaticMeshFromBrush(
			Pkg, 
			ObjName,
			pmxMeshInfoPtr, 
			Materials
			/*, NULL, ConversionTempModel*/
			);

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewMesh);

	return NewActor;
}
