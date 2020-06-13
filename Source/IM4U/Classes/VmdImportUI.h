// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// Copyright 2017- BlackMa9. All Rights Reserved.

/**
* Copyright 2010 Autodesk, Inc. All Rights Reserved.
*
* Fbx Importer UI options.
*/

#pragma once
#include "VmdImportUI.generated.h"

/** Import mesh type */
UENUM()
enum EVMDImportType
{
	/** Select Animation if you'd like to import only animation. */
	VMDIT_Animation UMETA(DisplayName = "Animation"),
	VMDIT_MAX,
};

UCLASS(config = EditorUserSettings, AutoExpandCategories = (FTransform), HideCategories = Object, MinimalAPI)
class UVmdImportUI : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Whether or not the imported file is in OBJ format */
	bool bIsObjImport;

	/** The original detected type of this import */
	TEnumAsByte<enum EVMDImportType> OriginalImportType;

	/** Type of asset to import from the FBX file */
	TEnumAsByte<enum EVMDImportType> MeshTypeToImport;

	/** Use the string in "Name" field as full name of mesh. The option only works when the scene contains one mesh. */
	uint32 bOverrideFullName : 1;

	/** Skeleton to use for imported asset. When importing a mesh, leaving this as "None" will create a new skeleton. When importing and animation this MUST be specified to import the asset. */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (OBJRestrict = "false"))
		class USkeleton* Skeleton;

	/** SkeletonMesh to use for imported asset. When importing a mesh, leaving this as "None" will create a new skeleton. When importing and animation this MUST be specified to import the asset. */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (OBJRestrict = "false"))
		class USkeletalMesh* SkeletonMesh;

	/** True to import animations from the FBX File */
	uint32 bImportAnimations : 1;

	/** Override for the name of the animation to import **/
	FString AnimationName;

	
	/** Import data used when importing static meshes */
	class UMMDStaticMeshImportData* StaticMeshImportData;
		
	/** Import data used when importing skeletal meshes */
	class UMMDSkeletalMeshImportData* SkeletalMeshImportData;

	/** Type of asset to import from the FBX file */
	bool bPreserveLocalTransform;

	// Begin UObject Interface
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	// End UObject Interface

	/////////////////////////

	/** Whether to automatically create Unreal materials for materials found in the FBX scene */


	/** Skeleton to use for imported asset. When importing a mesh, leaving this as "None" will create a new skeleton. When importing and animation this MUST be specified to import the asset. */
	UPROPERTY(EditAnywhere, Category = Animation, meta = (OBJRestrict = "false"))
		class UAnimSequence* AnimSequenceAsset;

	/** Skeleton to use for imported asset. When importing a mesh, leaving this as "None" will create a new skeleton. When importing and animation this MUST be specified to import the asset. */
		TArray<class UMMDSkeletalMeshImportData*> TestArrayList;
	
	/** MMD2UE4NameTableRow to use for imported asset. When importing a Anim, leaving this as "None" will create a new skeleton. When importing and animation this MUST be specified to import the asset. */
	UPROPERTY(EditAnywhere, Category = Animation, meta = (OBJRestrict = "false"))
		UDataTable*  MMD2UE4NameTableRow;

	/** mmd extend assset to use for calc ik . */
	class UMMDExtendAsset*  MmdExtendAsset;
};



