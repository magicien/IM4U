#include "IM4UPrivatePCH.h"

#include "CoreMinimal.h"

#include "Factories.h"
#include "Engine.h"
#include "SkelImport.h"
#include "PmxImporter.h"
#include "PmxFactory.h"
#include "PmxOptionWindow.h"
#include "MainFrame.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#include "MMDSkeletalMeshImportData.h"
#include "MMDStaticMeshImportData.h"

#define LOCTEXT_NAMESPACE "PmxMainImport"

PMXImportOptions* GetImportOptions(
	class FPmxImporter* PmxImporter,
	UPmxImportUI* ImportUI,
	bool bShowOptionDialog,
	const FString& FullPath,
	bool& bOutOperationCanceled,
	bool& bOutImportAll,
	bool bIsObjFormat,
	bool bForceImportType,
	EPMXImportType ImportType 
	)
{
	bOutOperationCanceled = false;

	if (bShowOptionDialog)
	{
		bOutImportAll = false;

		PMXImportOptions* ImportOptions 
			= PmxImporter->GetImportOptions();
		// if Skeleton was set by outside, please make sure copy back to UI
		if (ImportOptions->SkeletonForAnimation)
		{
			ImportUI->Skeleton = ImportOptions->SkeletonForAnimation;
		}
		else
		{
			ImportUI->Skeleton = NULL;
		}

		if (ImportOptions->PhysicsAsset)
		{
			ImportUI->PhysicsAsset = ImportOptions->PhysicsAsset;
		}
		else
		{
			ImportUI->PhysicsAsset = NULL;
		}
		if (bForceImportType)
		{
			ImportUI->MeshTypeToImport = ImportType;
			ImportUI->OriginalImportType = ImportType;
		}

		//last select asset ref
		if (ImportOptions->MmdExtendAsset)
		{
			ImportUI->MmdExtendAsset = ImportOptions->MmdExtendAsset;
		}
		else
		{
			ImportUI->MmdExtendAsset = NULL;
		}
		if (ImportOptions->MMD2UE4NameTableRow)
		{
			ImportUI->MMD2UE4NameTableRow = ImportOptions->MMD2UE4NameTableRow;
		}
		else
		{
			ImportUI->MMD2UE4NameTableRow = NULL;
		}
		if (ImportOptions->AnimSequenceAsset)
		{
			ImportUI->AnimSequenceAsset = ImportOptions->AnimSequenceAsset;
		}
		else
		{
			ImportUI->AnimSequenceAsset = NULL;
		}

		ImportUI->bImportAsSkeletal = ImportUI->MeshTypeToImport == PMXIT_SkeletalMesh;
		ImportUI->bIsObjImport = bIsObjFormat;

		TSharedPtr<SWindow> ParentWindow;

		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			//.Title(NSLOCTEXT("UnrealEd", "FBXImportOpionsTitle", "FBX Import Options"))
			.Title(NSLOCTEXT("IM4U", "MMDImportOpionsTitle", "MMD Import Options"))
			.SizingRule(ESizingRule::Autosized);

		TSharedPtr<SPmxOptionWindow> PmxOptionWindow;
		Window->SetContent
			(
			SAssignNew(PmxOptionWindow, SPmxOptionWindow)
			.ImportUI(ImportUI)
			.WidgetWindow(Window)
			.FullPath(FText::FromString(FullPath))
			.ForcedImportType(bForceImportType ? TOptional<EPMXImportType>(ImportType) : TOptional<EPMXImportType>())
			.IsObjFormat(bIsObjFormat)
			);

		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportUI->SaveConfig();

		if (ImportUI->StaticMeshImportData)
		{
			ImportUI->StaticMeshImportData->SaveConfig();
		}

		if (ImportUI->SkeletalMeshImportData)
		{
			ImportUI->SkeletalMeshImportData->SaveConfig();
		}

		if (PmxOptionWindow->ShouldImport())
		{
			bOutImportAll = PmxOptionWindow->ShouldImportAll();

			// open dialog
			// see if it's canceled
			ApplyImportUIToImportOptions(ImportUI, *ImportOptions);

			return ImportOptions;
		}
		else
		{
			bOutOperationCanceled = true;
		}
	}
	else if (GIsAutomationTesting)
	{
		//Automation tests set ImportUI settings directly.  Just copy them over
		PMXImportOptions* ImportOptions = PmxImporter->GetImportOptions();
		ApplyImportUIToImportOptions(ImportUI, *ImportOptions);
		return ImportOptions;
	}
	else
	{
		return PmxImporter->GetImportOptions();
	}
	return NULL;

}

void ApplyImportUIToImportOptions(
	UPmxImportUI* ImportUI,
	PMXImportOptions& InOutImportOptions
	)
{
	check(ImportUI);

	InOutImportOptions.bImportMaterials = ImportUI->bImportMaterials;
	//InOutImportOptions.bInvertNormalMap = ImportUI->TextureImportData->bInvertNormalMaps;
	InOutImportOptions.bImportTextures = ImportUI->bImportTextures;
	InOutImportOptions.bCreateMaterialInstMode = ImportUI->bCreateMaterialInstMode;
	InOutImportOptions.bUnlitMaterials = ImportUI->bUnlitMaterials;
	InOutImportOptions.bUsedAsFullName = ImportUI->bOverrideFullName;
	InOutImportOptions.bConvertScene = ImportUI->bConvertScene;
	InOutImportOptions.bImportAnimations = ImportUI->bImportAnimations;
#if 1
	InOutImportOptions.SkeletonForAnimation = ImportUI->Skeleton;
#endif
	if (ImportUI->MeshTypeToImport == PMXIT_StaticMesh)
	{
		UMMDStaticMeshImportData* StaticMeshData = ImportUI->StaticMeshImportData;
		InOutImportOptions.NormalImportMethod = StaticMeshData->NormalImportMethod;
		InOutImportOptions.ImportTranslation = StaticMeshData->ImportTranslation;
		InOutImportOptions.ImportRotation = StaticMeshData->ImportRotation;
		InOutImportOptions.ImportUniformScale = StaticMeshData->ImportUniformScale;
	}
	else if (ImportUI->MeshTypeToImport == PMXIT_SkeletalMesh)
	{
		UMMDSkeletalMeshImportData* SkeletalMeshData = ImportUI->SkeletalMeshImportData;
		InOutImportOptions.NormalImportMethod = SkeletalMeshData->NormalImportMethod;
		InOutImportOptions.ImportTranslation = SkeletalMeshData->ImportTranslation;
		InOutImportOptions.ImportRotation = SkeletalMeshData->ImportRotation;
		InOutImportOptions.ImportUniformScale = SkeletalMeshData->ImportUniformScale;
	}
	//add self pre over write..
	ImportUI->SkeletalMeshImportData->bImportMorphTargets = ImportUI->bImportMorphTargets;
	// only re-sample if they don't want to use default sample rate
	InOutImportOptions.bResample = ImportUI->bUseDefaultSampleRate == false;
	InOutImportOptions.bImportMorph = ImportUI->SkeletalMeshImportData->bImportMorphTargets;
	InOutImportOptions.bUpdateSkeletonReferencePose = ImportUI->SkeletalMeshImportData->bUpdateSkeletonReferencePose;
	InOutImportOptions.bImportRigidMesh = ImportUI->OriginalImportType == PMXIT_StaticMesh && ImportUI->MeshTypeToImport == PMXIT_SkeletalMesh;
	InOutImportOptions.bUseT0AsRefPose = ImportUI->SkeletalMeshImportData->bUseT0AsRefPose;
	InOutImportOptions.bPreserveSmoothingGroups = ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups;
	InOutImportOptions.bKeepOverlappingVertices = ImportUI->SkeletalMeshImportData->bKeepOverlappingVertices;
	InOutImportOptions.bCombineToSingle = ImportUI->bCombineMeshes;
	InOutImportOptions.VertexColorImportOption = ImportUI->StaticMeshImportData->VertexColorImportOption;
	InOutImportOptions.VertexOverrideColor = ImportUI->StaticMeshImportData->VertexOverrideColor;
	InOutImportOptions.bRemoveDegenerates = ImportUI->StaticMeshImportData->bRemoveDegenerates;
	InOutImportOptions.bGenerateLightmapUVs = ImportUI->StaticMeshImportData->bGenerateLightmapUVs;
	InOutImportOptions.bOneConvexHullPerUCX = ImportUI->StaticMeshImportData->bOneConvexHullPerUCX;
	InOutImportOptions.bAutoGenerateCollision = ImportUI->StaticMeshImportData->bAutoGenerateCollision;
	InOutImportOptions.StaticMeshLODGroup = ImportUI->StaticMeshImportData->StaticMeshLODGroup;
	InOutImportOptions.bImportMeshesInBoneHierarchy = ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy;
	InOutImportOptions.bCreatePhysicsAsset = ImportUI->bCreatePhysicsAsset;
	InOutImportOptions.PhysicsAsset = ImportUI->PhysicsAsset;

	//add self
	InOutImportOptions.AnimSequenceAsset = ImportUI->AnimSequenceAsset;
	InOutImportOptions.MMD2UE4NameTableRow = ImportUI->MMD2UE4NameTableRow;
	InOutImportOptions.MmdExtendAsset = ImportUI->MmdExtendAsset;
}

TSharedPtr<FPmxImporter> FPmxImporter::StaticInstance;
////////////////////////////////////////////
FPmxImporter::FPmxImporter()
	: ImportOptions(NULL)
{
	ImportOptions = new PMXImportOptions();
	FMemory::Memzero(*ImportOptions);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FPmxImporter::~FPmxImporter()
{
	CleanUp();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FPmxImporter* FPmxImporter::GetInstance()
{
	if (!StaticInstance.IsValid())
	{
		StaticInstance = MakeShareable(new FPmxImporter());
	}
	return StaticInstance.Get();
}

void FPmxImporter::DeleteInstance()
{
	StaticInstance.Reset();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void FPmxImporter::CleanUp()
{
	delete ImportOptions;
	ImportOptions = NULL;
}

PMXImportOptions* FPmxImporter::GetImportOptions() const
{
	return ImportOptions;
}

///////////////////////////////////////////////////////////////////////////

UPmxImportUI::UPmxImportUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)//, MMD2UE4NameTableRow(MMD2UE4NameTableRowDmmy)
{
	bCombineMeshes = true;

	StaticMeshImportData = CreateDefaultSubobject<UMMDStaticMeshImportData>(TEXT("StaticMeshImportData"));
	SkeletalMeshImportData = CreateDefaultSubobject<UMMDSkeletalMeshImportData>(TEXT("SkeletalMeshImportData"));
}


bool UPmxImportUI::CanEditChange(const UProperty* InProperty) const
{
	bool bIsMutable = Super::CanEditChange(InProperty);
	if (bIsMutable && InProperty != NULL)
	{
		FName PropName = InProperty->GetFName();

		if (PropName == TEXT("StartFrame") || PropName == TEXT("EndFrame"))
		{
		}
		else if (PropName == TEXT("bImportCustomAttribute") || PropName == TEXT("AnimationLength"))
		{
			bIsMutable = bImportAnimations;
		}

		if (bIsObjImport == false && InProperty->GetBoolMetaData(TEXT("OBJRestrict")))
		{
			bIsMutable = false;
		}
	}

	return bIsMutable;
}


#undef LOCTEXT_NAMESPACE
