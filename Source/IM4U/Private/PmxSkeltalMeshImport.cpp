#include "IM4UPrivatePCH.h"


#include "CoreMinimal.h"
#include "Factories.h"
#include "BusyCursor.h"
#include "SSkeletonWidget.h"

#include "Misc/FbxErrors.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"

/////////////////////////

#include "Engine.h"
#include "TextureLayout.h"
#include "SkelImport.h"
#include "AnimEncoding.h"
#include "SSkeletonWidget.h"

#include "AssetRegistryModule.h"
#include "AssetNotifications.h"

#include "ObjectTools.h"

#include "ApexClothingUtils.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"

#include "Animation/MorphTarget.h"
#include "ComponentReregisterContext.h"
////////////

#include "PmxFactory.h"
#define LOCTEXT_NAMESPACE "PMXSkeltalMeshImpoter"

////////////////////////////////////////////////////////////////////////////////////////////////
// FMorphMeshRawSource is removed after version 4.16. So added for only this plugin here.
// Converts a mesh to raw vertex data used to generate a morph target mesh

/** compare based on base mesh source vertex indices */
struct FCompareMorphTargetDeltas
{
	FORCEINLINE bool operator()(const FMorphTargetDelta& A, const FMorphTargetDelta& B) const
	{
		return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0 ? true : false;
	}
};

class FMorphMeshRawSource
{
public:
	struct FMorphMeshVertexRaw
	{
		FVector			Position;

		// Tangent, U-direction
		FVector			TangentX;
		// Binormal, V-direction
		FVector			TangentY;
		// Normal
		FVector4		TangentZ;
	};

	/** vertex data used for comparisons */
	TArray<FMorphMeshVertexRaw> Vertices;

	/** index buffer used for comparison */
	TArray<uint32> Indices;

	/** indices to original imported wedge points */
	TArray<uint32> WedgePointIndices;

	/** Constructor (default) */
	FMorphMeshRawSource() { }
	FMorphMeshRawSource(USkeletalMesh* SrcMesh, int32 LODIndex = 0);
	FMorphMeshRawSource(FSkeletalMeshLODModel& LODModel);

	static void CalculateMorphTargetLODModel(const FMorphMeshRawSource& BaseSource,
		const FMorphMeshRawSource& TargetSource, FMorphTargetLODModel& MorphModel);

private:
	void Initialize(FSkeletalMeshLODModel& LODModel);
};

/**
* Constructor.
* Converts a skeletal mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source skeletal mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource(USkeletalMesh* SrcMesh, int32 LODIndex)
{
	check(SrcMesh);
	check(SrcMesh->GetImportedModel());
	check(SrcMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex));

	// get the mesh data for the given lod
	FSkeletalMeshLODModel& LODModel = SrcMesh->GetImportedModel()->LODModels[LODIndex];

	Initialize(LODModel);
}

FMorphMeshRawSource::FMorphMeshRawSource(FSkeletalMeshLODModel& LODModel)
{
	Initialize(LODModel);
}

void FMorphMeshRawSource::Initialize(FSkeletalMeshLODModel& LODModel)
{
	// iterate over the chunks for the skeletal mesh
	for (int32 SectionIdx = 0; SectionIdx < LODModel.Sections.Num(); SectionIdx++)
	{
		const FSkelMeshSection& Section = LODModel.Sections[SectionIdx];
		for (int32 VertexIdx = 0; VertexIdx < Section.SoftVertices.Num(); VertexIdx++)
		{
			const FSoftSkinVertex& SourceVertex = Section.SoftVertices[VertexIdx];
			FMorphMeshVertexRaw RawVertex =
			{
				SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.Add(RawVertex);
		}
	}

	// Copy the indices manually, since the LODModel's index buffer may have a different alignment.
	Indices.Empty(LODModel.IndexBuffer.Num());
	for (int32 Index = 0; Index < LODModel.IndexBuffer.Num(); Index++)
	{
		Indices.Add(LODModel.IndexBuffer[Index]);
	}

	// copy the wedge point indices
	if (LODModel.RawPointIndices.GetBulkDataSize())
	{
		WedgePointIndices.Empty(LODModel.RawPointIndices.GetElementCount());
		WedgePointIndices.AddUninitialized(LODModel.RawPointIndices.GetElementCount());
		FMemory::Memcpy(WedgePointIndices.GetData(), LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize());
		LODModel.RawPointIndices.Unlock();
	}
}

void FMorphMeshRawSource::CalculateMorphTargetLODModel(const FMorphMeshRawSource& BaseSource,
	const FMorphMeshRawSource& TargetSource, FMorphTargetLODModel& MorphModel)
{
	// set the original number of vertices
	MorphModel.NumBaseMeshVerts = BaseSource.Vertices.Num();

	// empty morph mesh vertices first
	MorphModel.Vertices.Empty();

	// array to mark processed base vertices
	TArray<bool> WasProcessed;
	WasProcessed.Empty(BaseSource.Vertices.Num());
	WasProcessed.AddZeroed(BaseSource.Vertices.Num());


	TMap<uint32, uint32> WedgePointToVertexIndexMap;
	// Build a mapping of wedge point indices to vertex indices for fast lookup later.
	for (int32 Idx = 0; Idx < TargetSource.WedgePointIndices.Num(); Idx++)
	{
		WedgePointToVertexIndexMap.Add(TargetSource.WedgePointIndices[Idx], Idx);
	}

	// iterate over all the base mesh indices
	for (int32 Idx = 0; Idx < BaseSource.Indices.Num(); Idx++)
	{
		uint32 BaseVertIdx = BaseSource.Indices[Idx];

		// check for duplicate processing
		if (!WasProcessed[BaseVertIdx])
		{
			// mark this base vertex as already processed
			WasProcessed[BaseVertIdx] = true;

			// get base mesh vertex using its index buffer
			const FMorphMeshVertexRaw& VBase = BaseSource.Vertices[BaseVertIdx];

			// clothing can add extra verts, and we won't have source point, so we ignore those
			if (BaseSource.WedgePointIndices.IsValidIndex(BaseVertIdx))
			{
				// get the base mesh's original wedge point index
				uint32 BasePointIdx = BaseSource.WedgePointIndices[BaseVertIdx];

				// find the matching target vertex by searching for one
				// that has the same wedge point index
				uint32* TargetVertIdx = WedgePointToVertexIndexMap.Find(BasePointIdx);

				// only add the vertex if the source point was found
				if (TargetVertIdx != NULL)
				{
					// get target mesh vertex using its index buffer
					const FMorphMeshVertexRaw& VTarget = TargetSource.Vertices[*TargetVertIdx];

					// change in position from base to target
					FVector PositionDelta(VTarget.Position - VBase.Position);
					FVector NormalDeltaZ(VTarget.TangentZ - VBase.TangentZ);

					// check if position actually changed much
					if (PositionDelta.SizeSquared() > FMath::Square(THRESH_POINTS_ARE_NEAR) ||
						// since we can't get imported morphtarget normal from FBX
						// we can't compare normal unless it's calculated
						// this is special flag to ignore normal diff
						(true && NormalDeltaZ.SizeSquared() > 0.01f))
					{
						// create a new entry
						FMorphTargetDelta NewVertex;
						// position delta
						NewVertex.PositionDelta = PositionDelta;
						// normal delta
						NewVertex.TangentZDelta = NormalDeltaZ;
						// index of base mesh vert this entry is to modify
						NewVertex.SourceIdx = BaseVertIdx;

						// add it to the list of changed verts
						MorphModel.Vertices.Add(NewVertex);
					}
				}
			}
		}
	}

	// sort the array of vertices for this morph target based on the base mesh indices
	// that each vertex is associated with. This allows us to sequentially traverse the list
	// when applying the morph blends to each vertex.
	MorphModel.Vertices.Sort(FCompareMorphTargetDeltas());

	// remove array slack
	MorphModel.Vertices.Shrink();
}

////////////////////////////////////////////////////////////////////////////////////////////////
// UPmxFactory

bool UPmxFactory::ImportBone(
	//TArray<FbxNode*>& NodeArray,
	MMD4UE4::PmxMeshInfo *PmxMeshInfo,
	FSkeletalMeshImportData &ImportData,
	//UFbxSkeletalMeshImportData* TemplateData,
	//TArray<FbxNode*> &SortedLinks,
	bool& bOutDiffPose,
	bool bDisableMissingBindPoseWarning,
	bool & bUseTime0AsRefPose
	)
{
	bool GlobalLinkFoundFlag;

	bool bAnyLinksNotInBindPose = false;
	FString LinksWithoutBindPoses;
	int32 NumberOfRoot = 0;

	int32 RootIdx = -1;

	for (int LinkIndex = 0; LinkIndex<PmxMeshInfo->boneList.Num(); LinkIndex++)
	{
		// Add a bone for each FBX Link
		ImportData.RefBonesBinary.Add(SkeletalMeshImportData::FBone());

		//Link = SortedLinks[LinkIndex];

		// get the link parent and children
		int32 ParentIndex = INDEX_NONE; // base value for root if no parent found
		int32 LinkParent = PmxMeshInfo->boneList[LinkIndex].ParentBoneIndex;
		if (LinkIndex)
		{
			for (int32 ll = 0; ll<LinkIndex; ++ll) // <LinkIndex because parent is guaranteed to be before child in sortedLink
			{
				int32 Otherlink = ll;
				if (Otherlink == LinkParent)
				{
					ParentIndex = ll;
					break;
				}
			}
		}

		// see how many root this has
		// if more than 
		if (ParentIndex == INDEX_NONE)
		{
			++NumberOfRoot;
			RootIdx = LinkIndex;
			if (NumberOfRoot > 1)
			{
				AddTokenizedErrorMessage(
					FTokenizedMessage::Create(
						EMessageSeverity::Error,
						LOCTEXT("MultipleRootsFound", "Multiple roots are found in the bone hierarchy. We only support single root bone.")), 
					FFbxErrors::SkeletalMesh_MultipleRoots
					);
				return false;
			}
		}

		GlobalLinkFoundFlag = false;

		// set bone
		SkeletalMeshImportData::FBone& Bone = ImportData.RefBonesBinary[LinkIndex];
		FString BoneName;

		/*const char* LinkName = Link->GetName();
		BoneName = ANSI_TO_TCHAR(MakeName(LinkName));*/
		BoneName = PmxMeshInfo->boneList[LinkIndex].Name;//For MMD
		Bone.Name = BoneName;

		SkeletalMeshImportData::FJointPos& JointMatrix = Bone.BonePos;
		JointMatrix.Length = 1.;
		JointMatrix.XSize = 100.;
		JointMatrix.YSize = 100.;
		JointMatrix.ZSize = 100.;

		// get the link parent and children
		Bone.ParentIndex = ParentIndex;
		Bone.NumChildren = 0;
		for (int32 ChildIndex = 0; ChildIndex < PmxMeshInfo->boneList.Num(); ChildIndex++)
		{
			if (LinkIndex == PmxMeshInfo->boneList[ChildIndex].ParentBoneIndex)
			{
				Bone.NumChildren++;
			}
		}

		FVector TransTemp;
		if (ParentIndex != INDEX_NONE)
		{
			TransTemp = PmxMeshInfo->boneList[ParentIndex].Position
				- PmxMeshInfo->boneList[LinkIndex].Position;
			TransTemp *= -1;
		}
		else
		{
			TransTemp = PmxMeshInfo->boneList[LinkIndex].Position;
		}
		JointMatrix.Transform.SetTranslation(TransTemp);
		JointMatrix.Transform.SetRotation(FQuat(0,0,0,1.0));
		JointMatrix.Transform.SetScale3D(FVector(1));
	}

	return true;
}


bool UPmxFactory::FillSkelMeshImporterFromFbx(
	FSkeletalMeshImportData& ImportData,
	MMD4UE4::PmxMeshInfo *& PmxMeshInfo
	//FbxMesh*& Mesh,
	//FbxSkin* Skin,
	//FbxShape* FbxShape,
	//TArray<FbxNode*> &SortedLinks,
	//const TArray<FbxSurfaceMaterial*>& FbxMaterials
	)
{
	TArray<UMaterialInterface*> Materials;

	////////
	TArray<UTexture*> textureAssetList;
	if (ImportUI->bImportTextures)
	{
		for (int k = 0; k < PmxMeshInfo->textureList.Num(); ++k)
		{
			pmxMaterialImportHelper.AssetsCreateTextuer(
				//InParent,
				//Flags,
				//Warn,
				FPaths::GetPath(GetCurrentFilename()),
				PmxMeshInfo->textureList[k].TexturePath,
				textureAssetList
				);

			//if (NewObject)
			/*{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
				}*/
		}
	}
	//UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Texture Extecd Complete."));
	////////////////////////////////////////////

	TArray<FString> UVSets;
	if (ImportUI->bImportMaterials)
	{
		for (int k = 0; k < PmxMeshInfo->materialList.Num(); ++k)
		{
			pmxMaterialImportHelper.CreateUnrealMaterial(
				PmxMeshInfo->modelNameJP,
				//InParent,
				PmxMeshInfo->materialList[k],
				ImportUI->bCreateMaterialInstMode,
				ImportUI->bUnlitMaterials,
				Materials,
				textureAssetList);
			//if (NewObject)
			/*{
				NodeIndex++;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeIndex"), NodeIndex);
				Args.Add(TEXT("ArrayLength"), NodeIndexMax);// SkelMeshArray.Num());
				GWarn->StatusUpdate(NodeIndex, NodeIndexMax, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
				}*/
			{
				int ExistingMatIndex = k;
				int MaterialIndex = k;

				// material asset set flag for morph target 
				if (ImportUI->bImportMorphTargets)
				{
					if (UMaterial* UnrealMaterialPtr = Cast<UMaterial>(Materials[MaterialIndex]))
					{
						UnrealMaterialPtr->bUsedWithMorphTargets = 1;
					}
				}

				ImportData.Materials[ExistingMatIndex].MaterialImportName
					= "M_" + PmxMeshInfo->materialList[k].Name;
				ImportData.Materials[ExistingMatIndex].Material
					= Materials[MaterialIndex];
			}
		}
	}
	///////////////////////////////////////////
	//UE_LOG(LogCategoryPMXFactory, Warning, TEXT("PMX Import Material Extecd Complete."));
	///////////////////////////////////////////

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	uint32 UniqueUVCount = UVSets.Num();

	UniqueUVCount = FMath::Min<uint32>(UniqueUVCount, MAX_TEXCOORDS);
	// One UV set is required but only import up to MAX_TEXCOORDS number of uv layers
	ImportData.NumTexCoords = FMath::Max<uint32>(ImportData.NumTexCoords, UniqueUVCount);

	ImportData.bHasNormals = false;
	ImportData.bHasTangents = true;

	//
	// create the points / wedges / faces
	//
	int32 ControlPointsCount =
	PmxMeshInfo->vertexList.Num();
	int32 ExistPointNum = ImportData.Points.Num();
	ImportData.Points.AddUninitialized(ControlPointsCount);

	int32 ControlPointsIndex;
	for (ControlPointsIndex = 0; ControlPointsIndex < ControlPointsCount; ControlPointsIndex++)
	{
		ImportData.Points[ControlPointsIndex + ExistPointNum]
			= PmxMeshInfo->vertexList[ControlPointsIndex].Position;
	}

	bool OddNegativeScale = true;// false;// IsOddNegativeScale(TotalMatrix);

	int32 VertexIndex;
	int32 TriangleCount = PmxMeshInfo->faseList.Num();//Mesh->GetPolygonCount();
	int32 ExistFaceNum = ImportData.Faces.Num();
	ImportData.Faces.AddUninitialized(TriangleCount);
	int32 ExistWedgesNum = ImportData.Wedges.Num();
	SkeletalMeshImportData::FVertex TmpWedges[3];

	int32 facecount =0;
	int32 matIndx = 0;

	for (int32 TriangleIndex = ExistFaceNum, LocalIndex = 0; TriangleIndex < ExistFaceNum + TriangleCount; TriangleIndex++, LocalIndex++)
	{

		SkeletalMeshImportData::FTriangle& Triangle = ImportData.Faces[TriangleIndex];

		//
		// smoothing mask
		//
		// set the face smoothing by default. It could be any number, but not zero
		Triangle.SmoothingGroups = 255;

		for (VertexIndex = 0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			int32 NormalIndex = UnrealVertexIndex;

			FVector TangentZ 
				= PmxMeshInfo->vertexList[PmxMeshInfo->faseList[LocalIndex].VertexIndex[NormalIndex]].Normal;

			Triangle.TangentX[NormalIndex] = FVector::ZeroVector;
			Triangle.TangentY[NormalIndex] = FVector::ZeroVector;
			Triangle.TangentZ[NormalIndex] = TangentZ.GetSafeNormal();
		}

		//
		// material index
		//
		Triangle.MatIndex = 0; // default value

		if (PmxMeshInfo->materialList.Num() > matIndx)
		{
			facecount++;
			facecount++;
			facecount++;
			Triangle.MatIndex = matIndx;
			if (facecount >= PmxMeshInfo->materialList[matIndx].MaterialFaceNum)
			{
				matIndx++;
				facecount = 0;
			}
		}

		Triangle.AuxMatIndex = 0;
		for (VertexIndex = 0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			TmpWedges[UnrealVertexIndex].MatIndex = Triangle.MatIndex;
			TmpWedges[UnrealVertexIndex].VertexIndex
				= PmxMeshInfo->faseList[LocalIndex].VertexIndex[VertexIndex];
				// = ExistPointNum + Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
			// Initialize all colors to white.
			TmpWedges[UnrealVertexIndex].Color = FColor::White;
		}

		//
		// uvs
		//
		uint32 UVLayerIndex;
		// Some FBX meshes can have no UV sets, so also check the UniqueUVCount
		for (UVLayerIndex = 0; UVLayerIndex< UniqueUVCount; UVLayerIndex++)
		{
				// Set all UV's to zero.  If we are here the mesh had no UV sets so we only need to do this for the
				// first UV set which always exists.
				TmpWedges[VertexIndex].UVs[UVLayerIndex].X = 0;
				TmpWedges[VertexIndex].UVs[UVLayerIndex].Y = 0;
		}

		//
		// basic wedges matching : 3 unique per face. TODO Can we do better ?
		//
		for (VertexIndex = 0; VertexIndex<3; VertexIndex++)
		{
			int32 w;

			w = ImportData.Wedges.AddUninitialized();
			ImportData.Wedges[w].VertexIndex = TmpWedges[VertexIndex].VertexIndex;
			ImportData.Wedges[w].MatIndex = TmpWedges[VertexIndex].MatIndex;
			ImportData.Wedges[w].Color = TmpWedges[VertexIndex].Color;
			ImportData.Wedges[w].Reserved = 0;

			FVector2D tempUV 
				= PmxMeshInfo->vertexList[TmpWedges[VertexIndex].VertexIndex].UV;
			TmpWedges[VertexIndex].UVs[0].X = tempUV.X;
			TmpWedges[VertexIndex].UVs[0].Y = tempUV.Y;
			FMemory::Memcpy(ImportData.Wedges[w].UVs, 
				TmpWedges[VertexIndex].UVs, 
				sizeof(FVector2D)*MAX_TEXCOORDS);

			Triangle.WedgeIndex[VertexIndex] = w;
		}

	}

	if (PmxMeshInfo->boneList.Num() > 0)
	{
		// create influences for each cluster
		//	for each vertex index in the cluster
		for (int32 ControlPointIndex = 0;
			ControlPointIndex < PmxMeshInfo->vertexList.Num();
			++ControlPointIndex)
		{
			int32 multiBone = 0;
			switch (PmxMeshInfo->vertexList[ControlPointIndex].WeightType)
			{
			case 0://0:BDEF1
				{
					ImportData.Influences.AddUninitialized();
					ImportData.Influences.Last().BoneIndex = PmxMeshInfo->vertexList[ControlPointIndex].BoneIndex[0];
					ImportData.Influences.Last().Weight = PmxMeshInfo->vertexList[ControlPointIndex].BoneWeight[0];
					ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
				}
				break;
			case 1:// 1:BDEF2
				{
					for (multiBone = 0; multiBone < 2; ++multiBone)
					{
						ImportData.Influences.AddUninitialized();
						ImportData.Influences.Last().BoneIndex = PmxMeshInfo->vertexList[ControlPointIndex].BoneIndex[multiBone];
						ImportData.Influences.Last().Weight = PmxMeshInfo->vertexList[ControlPointIndex].BoneWeight[multiBone];
						ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
					}
					//UE_LOG(LogMMD4UE4_PMXFactory, Log, TEXT("BDEF2"), "");
				}
				break;
			case 2: //2:BDEF4
				{
					for ( multiBone = 0; multiBone < 4; ++multiBone)
					{
						ImportData.Influences.AddUninitialized();
						ImportData.Influences.Last().BoneIndex = PmxMeshInfo->vertexList[ControlPointIndex].BoneIndex[multiBone];
						ImportData.Influences.Last().Weight = PmxMeshInfo->vertexList[ControlPointIndex].BoneWeight[multiBone];
						ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
					}
				}
				break;
			case 3: //3:SDEF
				{
					//制限事項：SDEF
					//SDEFに関してはBDEF2に変換して扱うとする。
					//これは、SDEF_C、SDEF_R0、SDEF_R1に関するパラメータを設定する方法が不明なため。
					//別PF(ex.MMD4MecanimやDxlib)での実装例を元に解析及び情報収集し、
					//かつ、MMDでのSDEF動作の仕様を満たす方法を見つけられるまで保留、
					//SDEFの仕様（MMD）に関しては以下のページを参考にすること。
					//Ref :： みくだん 各ソフトによるSDEF変形の差異 - FC2
					// http://mikudan.blog120.fc2.com/blog-entry-339.html

					/////////////////////////////////////
					for ( multiBone = 0; multiBone < 2; ++multiBone)
					{
						ImportData.Influences.AddUninitialized();
						ImportData.Influences.Last().BoneIndex = PmxMeshInfo->vertexList[ControlPointIndex].BoneIndex[multiBone];
						ImportData.Influences.Last().Weight = PmxMeshInfo->vertexList[ControlPointIndex].BoneWeight[multiBone];
						ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
					}
				}
				break;

			default:
				{
					//異常系
					//0:BDEF1 形式と同じ手法で暫定対応する
					ImportData.Influences.AddUninitialized();
					ImportData.Influences.Last().BoneIndex = PmxMeshInfo->vertexList[ControlPointIndex].BoneIndex[0];
					ImportData.Influences.Last().Weight = PmxMeshInfo->vertexList[ControlPointIndex].BoneWeight[0];
					ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
					UE_LOG(LogMMD4UE4_PMXFactory, Error, 
						TEXT("Unkown Weight Type :: type = %d , vertex index = %d "), 
						PmxMeshInfo->vertexList[ControlPointIndex].WeightType
						, ControlPointIndex
						);
				}
				break;
			}
		}

	}
	else // for rigid mesh
	{
		// find the bone index
		int32 BoneIndex = -1;
		/*for (int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
		{
			// the bone is the node itself
			if (Node == SortedLinks[LinkIndex])
			{
				BoneIndex = LinkIndex;
				break;
			}
		}*/
		BoneIndex = 0;
		//	for each vertex in the mesh
		for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointsCount; ++ControlPointIndex)
		{
			ImportData.Influences.AddUninitialized();
			ImportData.Influences.Last().BoneIndex = BoneIndex;
			ImportData.Influences.Last().Weight = 1.0;
			ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
		}
	}

	return true;
}


UObject* UPmxFactory::CreateAssetOfClass(
	UClass* AssetClass,
	FString ParentPackageName, 
	FString ObjectName,
	bool bAllowReplace
	)
{
	// See if this sequence already exists.
	UObject* 	ParentPkg = CreatePackage(NULL, *ParentPackageName);
	FString 	ParentPath = FString::Printf(
								TEXT("%s/%s"), 
								*FPackageName::GetLongPackagePath(*ParentPackageName),
								*ObjectName);
	UObject* 	Parent = CreatePackage(NULL, *ParentPath);
	// See if an object with this name exists
	UObject* Object = LoadObject<UObject>(Parent, *ObjectName, NULL, LOAD_None, NULL);

	// if object with same name but different class exists, warn user
	if ((Object != NULL) && (Object->GetClass() != AssetClass))
	{
		//UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		//FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_AssetExist", "Asset with same name exists. Can't overwrite another asset")), FFbxErrors::Generic_SameNameAssetExists);
		return NULL;
	}

	if (Object == NULL)
	{
		// add it to the set
		// do not add to the set, now create independent asset
		Object = NewObject<UObject>(Parent, AssetClass, *ObjectName, RF_Public | RF_Standalone);
		Object->MarkPackageDirty();
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Object);
	}

	return Object;
}


/**
* A class encapsulating morph target processing that occurs during import on a separate thread
*/
class FAsyncImportMorphTargetWork : public FNonAbandonableTask
{
public:
	FAsyncImportMorphTargetWork(
		USkeletalMesh* InTempSkelMesh,
		int32 InLODIndex,
		FSkeletalMeshImportData& InImportData, 
		bool bInKeepOverlappingVertices
		)
			: TempSkeletalMesh(InTempSkelMesh)
			, ImportData(InImportData)
			, LODIndex(InLODIndex)
			, bKeepOverlappingVertices(bInKeepOverlappingVertices)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	}

	void DoWork()
	{
		TArray<FVector> LODPoints;
		TArray<SkeletalMeshImportData::FMeshWedge> LODWedges;
		TArray<SkeletalMeshImportData::FMeshFace> LODFaces;
		TArray<SkeletalMeshImportData::FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		ImportData.CopyLODImportData(LODPoints, LODWedges, LODFaces, LODInfluences, LODPointToRawMap);

		ImportData.Empty();
		MeshUtilities->BuildSkeletalMesh(
			TempSkeletalMesh->GetImportedModel()->LODModels[0],
			TempSkeletalMesh->RefSkeleton,
			LODInfluences,
			LODWedges,
			LODFaces,
			LODPoints,
			LODPointToRawMap
		);
	}

	//UE4.7系まで
	static const TCHAR *Name()
	{
		return TEXT("FAsyncImportMorphTargetWork");
	}

	//UE4.8以降で利用する場合に必要
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncImportMorphTargetWork, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	USkeletalMesh* TempSkeletalMesh;
	FSkeletalMeshImportData ImportData;
	IMeshUtilities* MeshUtilities;
	int32 LODIndex;
	bool bKeepOverlappingVertices;
};

void UPmxFactory::ImportMorphTargetsInternal(
	//TArray<FbxNode*>& SkelMeshNodeArray,
	MMD4UE4::PmxMeshInfo & PmxMeshInfo,
	USkeletalMesh* BaseSkelMesh,
	UObject* InParent,
	const FString& InFilename,
	int32 LODIndex
	)
{
	/*FbxString ShapeNodeName;
	TMap<FString, TArray<FbxShape*>> ShapeNameToShapeArray;*/
	TMap<FString, MMD4UE4::PMX_MORPH>  ShapeNameToShapeArray;

	// Temp arrays to keep track of data being used by threads
	TArray<USkeletalMesh*> TempMeshes;
	TArray<UMorphTarget*> MorphTargets;

	// Array of pending tasks that are not complete
	TIndirectArray<FAsyncTask<FAsyncImportMorphTargetWork> > PendingWork;

	GWarn->BeginSlowTask(NSLOCTEXT("FbxImporter", "BeginGeneratingMorphModelsTask", "Generating Morph Models"), true);

	for (int32 NodeIndex = 0; NodeIndex < PmxMeshInfo.morphList.Num(); NodeIndex++)
	{
		MMD4UE4::PMX_MORPH * pmxMorphPtr = &(PmxMeshInfo.morphList[NodeIndex]);
		if (pmxMorphPtr->Type == 1 && pmxMorphPtr->Vertex.Num()>0)
		{//頂点Morph

			FString ShapeName = pmxMorphPtr->Name;
			MMD4UE4::PMX_MORPH & ShapeArray
				= ShapeNameToShapeArray.FindOrAdd(ShapeName);
			ShapeArray = *pmxMorphPtr;
		}
	}

	int32 ShapeIndex = 0;
	int32 TotalShapeCount = ShapeNameToShapeArray.Num();
	// iterate through shapename, and create morphtarget
	for (auto Iter = ShapeNameToShapeArray.CreateIterator(); Iter; ++Iter)
	{
		FString ShapeName = Iter.Key();
		MMD4UE4::PMX_MORPH & ShapeArray = Iter.Value();

		//FString ShapeName = PmxMeshInfo.morphList[0].Name;
		//copy pmx meta date -> to this morph marge vertex data
		MMD4UE4::PmxMeshInfo ShapePmxMeshInfo = PmxMeshInfo;
		for (int tempVertexIndx = 0; tempVertexIndx< ShapeArray.Vertex.Num(); tempVertexIndx++)
		{
			MMD4UE4::PMX_MORPH_VERTEX tempMorphVertex = ShapeArray.Vertex[tempVertexIndx];
			//
			check(tempMorphVertex.Index <= ShapePmxMeshInfo.vertexList.Num());
			ShapePmxMeshInfo.vertexList[tempMorphVertex.Index].Position
				+= tempMorphVertex.Offset;
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("ShapeName"), FText::FromString(ShapeName));
		Args.Add(TEXT("CurrentShapeIndex"), ShapeIndex + 1);
		Args.Add(TEXT("TotalShapes"), TotalShapeCount);
		const FText StatusUpate
			= FText::Format(
				NSLOCTEXT(
					"FbxImporter", 
					"GeneratingMorphTargetMeshStatus",
					"Generating morph target mesh {ShapeName} ({CurrentShapeIndex} of {TotalShapes})"
					),
				Args);

		GWarn->StatusUpdate(ShapeIndex + 1, TotalShapeCount, StatusUpate);

		UE_LOG(LogMMD4UE4_PMXFactory, Warning, TEXT("%d_%s"),__LINE__, *(StatusUpate.ToString()) );

		FSkeletalMeshImportData ImportData;

		// See if this morph target already exists.
		UMorphTarget * Result = FindObject<UMorphTarget>(BaseSkelMesh, *ShapeName);
		// we only create new one for LOD0, otherwise don't create new one
		if (!Result)
		{
			if (LODIndex == 0)
			{
				Result = NewObject<UMorphTarget>(BaseSkelMesh, FName(*ShapeName));
			}
			else
			{
				AddTokenizedErrorMessage(
					FTokenizedMessage::Create(
						EMessageSeverity::Error,
						FText::Format(
							FText::FromString(
								"Could not find the {0} morphtarget for LOD {1}. \
								Make sure the name for morphtarget matches with LOD 0"), 
							FText::FromString(ShapeName),
							FText::FromString(FString::FromInt(LODIndex))
						)
					),
					FFbxErrors::SkeletalMesh_LOD_MissingMorphTarget
					);
			}
		}

		if (Result)
		{
			//Test
			//PmxMeshInfo.vertexList[0].Position = FVector::ZeroVector;

			// now we get a shape for whole mesh, import to unreal as a morph target
			// @todo AssetImportData do we need import data for this temp mesh?
			USkeletalMesh* TmpSkeletalMesh 
				= (USkeletalMesh*)ImportSkeletalMesh(
					GetTransientPackage(),
					&ShapePmxMeshInfo,//PmxMeshInfo,//SkelMeshNodeArray, 
					NAME_None,
					(EObjectFlags)0, 
					//TmpMeshImportData,
					FPaths::GetBaseFilename(InFilename), 
					//&ShapeArray, 
					&ImportData,
					false
					);
			TempMeshes.Add(TmpSkeletalMesh);
			MorphTargets.Add(Result);

			
			// Process the skeletal mesh on a separate thread
			FAsyncTask<FAsyncImportMorphTargetWork>* NewWork
				= new (PendingWork)FAsyncTask<FAsyncImportMorphTargetWork>(
					TmpSkeletalMesh,
					LODIndex,
					ImportData,
					true// ImportOptions->bKeepOverlappingVertices
					);
			NewWork->StartBackgroundTask();
			
			++ShapeIndex;
		}
	}

	// Wait for all importing tasks to complete
	int32 NumCompleted = 0;
	int32 NumTasks = PendingWork.Num();
	do
	{
		// Check for completed async compression tasks.
		int32 NumNowCompleted = 0;
		for (int32 TaskIndex = 0; TaskIndex < PendingWork.Num(); ++TaskIndex)
		{
			if (PendingWork[TaskIndex].IsDone())
			{
				NumNowCompleted++;
			}
		}
		if (NumNowCompleted > NumCompleted)
		{
			NumCompleted = NumNowCompleted;
			FFormatNamedArguments Args;
			Args.Add(TEXT("NumCompleted"), NumCompleted);
			Args.Add(TEXT("NumTasks"), NumTasks);
			GWarn->StatusUpdate(
				NumCompleted,
				NumTasks, 
				FText::Format(
					LOCTEXT(
						"ImportingMorphTargetStatus",
						"Importing Morph Target: {NumCompleted} of {NumTasks}"),
					Args)
				);
		}
		FPlatformProcess::Sleep(0.1f);
	} while (NumCompleted < NumTasks);

	// Create morph streams for each morph target we are importing.
	// This has to happen on a single thread since the skeletal meshes' bulk data is locked and cant be accessed by multiple threads simultaneously
	for (int32 Index = 0; Index < TempMeshes.Num(); Index++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NumCompleted"), Index + 1);
		Args.Add(TEXT("NumTasks"), TempMeshes.Num());
		GWarn->StatusUpdate(
			Index + 1,
			TempMeshes.Num(),
			FText::Format(
				LOCTEXT("BuildingMorphTargetRenderDataStatus",
					"Building Morph Target Render Data: {NumCompleted} of {NumTasks}"),
				Args)
			);

		UMorphTarget* MorphTarget = MorphTargets[Index];
		USkeletalMesh* TmpSkeletalMesh = TempMeshes[Index];

		FMorphMeshRawSource TargetMeshRawData(TmpSkeletalMesh);
		FMorphMeshRawSource BaseMeshRawData(BaseSkelMesh, LODIndex);

		FSkeletalMeshLODModel & BaseLODModel = BaseSkelMesh->GetImportedModel()->LODModels[LODIndex];
		FMorphTargetLODModel Result;
		FMorphMeshRawSource::CalculateMorphTargetLODModel(BaseMeshRawData, TargetMeshRawData, Result);

		MorphTarget->PopulateDeltas(Result.Vertices, LODIndex, BaseLODModel.Sections, true);
		// register does mark package as dirty
		if (MorphTarget->HasValidData())
		{
			BaseSkelMesh->RegisterMorphTarget(MorphTarget);
		}
	}

	GWarn->EndSlowTask();
}

// Import Morph target
void UPmxFactory::ImportFbxMorphTarget(
	//TArray<FbxNode*> &SkelMeshNodeArray,
	MMD4UE4::PmxMeshInfo & PmxMeshInfo,
	USkeletalMesh* BaseSkelMesh, 
	UObject* InParent, 
	const FString& Filename, 
	int32 LODIndex
	)
{
	bool bHasMorph = false;
//	int32 NodeIndex;

	if (PmxMeshInfo.morphList.Num()>0)
	{
		ImportMorphTargetsInternal(
			//SkelMeshNodeArray, 
			PmxMeshInfo,
			BaseSkelMesh, 
			InParent,
			Filename, 
			LODIndex
			);
	}
}



//////////////////////////////

void UPmxFactory::AddTokenizedErrorMessage(
	TSharedRef<FTokenizedMessage> ErrorMsg, 
	FName FbxErrorName
	)
{
	switch (ErrorMsg->GetSeverity())
	{
		case EMessageSeverity::Error:
			UE_LOG(LogMMD4UE4_PMXFactory, Error, TEXT("%d_%s"), __LINE__, *(ErrorMsg->ToText().ToString()));
			break;
		case EMessageSeverity::CriticalError:
			UE_LOG(LogMMD4UE4_PMXFactory, Error, TEXT("%d_%s"), __LINE__, *(ErrorMsg->ToText().ToString()));
			break;
		case EMessageSeverity::Warning:
			UE_LOG(LogMMD4UE4_PMXFactory, Warning, TEXT("%d_%s"), __LINE__, *(ErrorMsg->ToText().ToString()));
			break;
		default:
			UE_LOG(LogMMD4UE4_PMXFactory, Warning, TEXT("%d_%s"), __LINE__, *(ErrorMsg->ToText().ToString()));
			break;
	}
}

#undef LOCTEXT_NAMESPACE
