// Copyright 2015 BlackMa9. All Rights Reserved.
#pragma once


#include "Engine.h"

#include "MMDImportHelper.h"
#include "PmxImportUI.h"
#include "MMDExtendAsset.h"

#include "MMDStaticMeshImportData.h"


/////////////////////////////////////////////////
// Copy From DxLib DxModelLoader4.h
// DX Library Copyright (C) 2001-2008 Takumi Yamada.

//#define uint8 (unsigned char)

namespace MMD4UE4
{

	// マクロ定義 -----------------------------------

	#define PMX_MAX_IKLINKNUM		(64)			// 対応するIKリンクの最大数

	// データ型定義 ---------------------------------

	// PMXファイルの情報を格納する構造体
	struct PMX_BASEINFO
	{
		uint8	EncodeType;							// 文字コードのエンコードタイプ 0:UTF16 1:UTF8
		uint8	UVNum;								// 追加ＵＶ数 ( 0～4 )
		uint8	VertexIndexSize;					// 頂点Indexサイズ ( 1 or 2 or 4 )
		uint8	TextureIndexSize;					// テクスチャIndexサイズ ( 1 or 2 or 4 )
		uint8	MaterialIndexSize;					// マテリアルIndexサイズ ( 1 or 2 or 4 )
		uint8	BoneIndexSize;						// ボーンIndexサイズ ( 1 or 2 or 4 )
		uint8	MorphIndexSize;						// モーフIndexサイズ ( 1 or 2 or 4 )
		uint8	RigidIndexSize;						// 剛体Indexサイズ ( 1 or 2 or 4 )
	};

	// 頂点データ
	struct PMX_VERTEX
	{
		FVector		Position;						// 座標
		FVector		Normal;							// 法線
		FVector2D	UV;								// 標準UV値
		FVector4	AddUV[4];						// 追加UV値
		uint8		WeightType;						// ウエイトタイプ( 0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF )
		uint32		BoneIndex[4];					// ボーンインデックス
		float		BoneWeight[4];					// ボーンウェイト
		FVector		SDEF_C;							// SDEF-C
		FVector		SDEF_R0;						// SDEF-R0
		FVector		SDEF_R1;						// SDEF-R1
		float		ToonEdgeScale;					// トゥーンエッジのスケール
	};

	// 面リスト
	struct PMX_FACE
	{
		uint32		VertexIndex[3];					//頂点IndexList
	};

	// テクスチャ情報
	struct PMX_TEXTURE
	{
		FString	TexturePath;
	};

	// マテリアル情報
	struct PMX_MATERIAL
	{
		FString	Name;						// 名前
		FString	NameEng;						// 名前

		float	Diffuse[4];						// ディフューズカラー
		float	Specular[3];						// スペキュラカラー
		float	SpecularPower;						// スペキュラ定数
		float	Ambient[3];						// アンビエントカラー

		uint8	CullingOff;						// 両面描画
		uint8	GroundShadow;						// 地面影
		uint8	SelfShadowMap;						// セルフシャドウマップへの描画
		uint8	SelfShadowDraw;					// セルフシャドウの描画
		uint8	EdgeDraw;							// エッジの描画

		float	EdgeColor[4];					// エッジカラー
		float	EdgeSize;							// エッジサイズ

		int		TextureIndex;						// 通常テクスチャインデックス
		int		SphereTextureIndex;				// スフィアテクスチャインデックス
		uint8	SphereMode;						// スフィアモード( 0:無効 1:乗算 2:加算 3:サブテクスチャ(追加UV1のx,yをUV参照して通常テクスチャ描画を行う)

		uint8	ToonFlag;							// 共有トゥーンフラグ( 0:個別Toon 1:共有Toon )
		int		ToonTextureIndex;					// トゥーンテクスチャインデックス

		int		MaterialFaceNum;					// マテリアルが適応されている面の数
	};

	// ＩＫリンク情報
	struct PMX_IKLINK
	{
		int		BoneIndex;							// リンクボーンのインデックス
		uint8	RotLockFlag;						// 回転制限( 0:OFF 1:ON )
		float	RotLockMin[3];					// 回転制限、下限
		float	RotLockMax[3];					// 回転制限、上限
	};

	// ＩＫ情報
	struct PMX_IK
	{
		int		TargetBoneIndex;					// IKターゲットのボーンインデックス
		int		LoopNum;							// IK計算のループ回数
		float	RotLimit;							// 計算一回辺りの制限角度

		int		LinkNum;							// ＩＫリンクの数
		PMX_IKLINK Link[PMX_MAX_IKLINKNUM];		// ＩＫリンク情報
	};

	// ボーン情報
	struct PMX_BONE
	{
		FString	Name;						// 名前
		FString	NameEng;						// 名前
		FVector	Position;						// 座標
		int		ParentBoneIndex;					// 親ボーンインデックス
		int		TransformLayer;					// 変形階層

		uint8	Flag_LinkDest;						// 接続先
		uint8	Flag_EnableRot;					// 回転ができるか
		uint8	Flag_EnableMov;					// 移動ができるか
		uint8	Flag_Disp;							// 表示
		uint8	Flag_EnableControl;				// 操作ができるか
		uint8	Flag_IK;							// IK
		uint8	Flag_AddRot;						// 回転付与
		uint8	Flag_AddMov;						// 移動付与
		uint8	Flag_LockAxis;						// 軸固定
		uint8	Flag_LocalAxis;					// ローカル軸
		uint8	Flag_AfterPhysicsTransform;		// 物理後変形
		uint8	Flag_OutParentTransform;			// 外部親変形

		FVector	OffsetPosition;				// オフセット座標
		int		LinkBoneIndex;						// 接続先ボーンインデックス
		int		AddParentBoneIndex;				// 付与親ボーンインデックス
		float	AddRatio;							// 付与率
		FVector	LockAxisVector;				// 軸固定時の軸の方向ベクトル
		FVector	LocalAxisXVector;				// ローカル軸のＸ軸
		FVector	LocalAxisZVector;				// ローカル軸のＺ軸
		int		OutParentTransformKey;				// 外部親変形時の Key値

		PMX_IK	IKInfo;							// ＩＫ情報
	};

	// 頂点モーフ情報
	struct PMX_MORPH_VERTEX
	{
		int		Index;								// 頂点インデックス
		FVector	Offset;						// 頂点座標オフセット
	};

	// ＵＶモーフ情報
	struct PMX_MORPH_UV
	{
		int		Index;								// 頂点インデックス
		float	Offset[4];							// 頂点ＵＶオフセット
	};

	// ボーンモーフ情報
	struct PMX_MORPH_BONE
	{
		int		Index;								// ボーンインデックス
		FVector	Offset;						// 座標オフセット
		float	Quat[4];							// 回転クォータニオン
	};

	// 材質モーフ情報
	struct PMX_MORPH_MATERIAL
	{
		int		Index;								// マテリアルインデックス
		uint8	CalcType;							// 計算タイプ( 0:乗算  1:加算 )
		float	Diffuse[4];						// ディフューズカラー
		float	Specular[3];						// スペキュラカラー
		float	SpecularPower;						// スペキュラ係数
		float	Ambient[3];						// アンビエントカラー
		float	EdgeColor[4];					// エッジカラー
		float	EdgeSize;							// エッジサイズ
		float	TextureScale[4];					// テクスチャ係数
		float	SphereTextureScale[4];			// スフィアテクスチャ係数
		float	ToonTextureScale[4];				// トゥーンテクスチャ係数
	};

	// グループモーフ
	struct PMX_MORPH_GROUP
	{
		int		Index;								// モーフインデックス
		float	Ratio;								// モーフ率
	};

	// モーフ情報
	struct PMX_MORPH
	{
		FString	Name;						// 名前
		FString	NameEng;						// 名前

		uint8	ControlPanel;						// 操作パネル
		uint8	Type;								// モーフの種類  0:グループ 1:頂点 2:ボーン 3:UV 4:追加UV1 5:追加UV2 6:追加UV3 7:追加UV4 8:材質

		int		DataNum;							// モーフ情報の数

		TArray<PMX_MORPH_VERTEX>	Vertex;				// 頂点モーフ
		TArray<PMX_MORPH_UV>		UV;						// UVモーフ
		TArray<PMX_MORPH_BONE>		Bone;					// ボーンモーフ
		TArray<PMX_MORPH_MATERIAL>	Material;			// マテリアルモーフ
		TArray<PMX_MORPH_GROUP>		Group;				// グループモーフ
	};

	// 剛体情報
	struct PMX_RIGIDBODY
	{
		FString	Name;						// 名前

		int		BoneIndex;							// 対象ボーン番号

		uint8	RigidBodyGroupIndex;				// 剛体グループ番号
		uint16	RigidBodyGroupTarget;				// 剛体グループ対象

		uint8	ShapeType;							// 形状( 0:球  1:箱  2:カプセル )
		float	ShapeW;							// 幅
		float	ShapeH;							// 高さ
		float	ShapeD;							// 奥行

		FVector	Position;						// 位置
		float	Rotation[3];						// 回転( ラジアン )

		float	RigidBodyWeight;					// 質量
		float	RigidBodyPosDim;					// 移動減衰
		float	RigidBodyRotDim;					// 回転減衰
		float	RigidBodyRecoil;					// 反発力
		float	RigidBodyFriction;					// 摩擦力

		uint8	RigidBodyType;						// 剛体タイプ( 0:Bone追従  1:物理演算  2:物理演算(Bone位置合わせ) )
	};

	// ジョイント情報
	struct PMX_JOINT
	{
		FString	Name;						// 名前

		uint8	Type;								// 種類( 0:スプリング6DOF ( PMX2.0 では 0 のみ )

		int		RigidBodyAIndex;					// 接続先剛体Ａ
		int		RigidBodyBIndex;					// 接続先剛体Ｂ

		FVector	Position;						// 位置
		float	Rotation[3];						// 回転( ラジアン )

		float	ConstrainPositionMin[3];			// 移動制限-下限
		float	ConstrainPositionMax[3];			// 移動制限-上限
		float	ConstrainRotationMin[3];			// 回転制限-下限
		float	ConstrainRotationMax[3];			// 回転制限-上限

		float	SpringPosition[3];				// バネ定数-移動
		float	SpringRotation[3];				// バネ定数-回転
	};
	//////////////////////////////////////////////////////////////

	struct PMX_BONE_HIERARCHY
	{
		int		originalBoneIndex;
		int		fixedBoneIndex;
		bool	fixFlag_Parent;
		//bool	fixFlag_Target;
	};
	//////////////////////////////////////////////////////////////

	DECLARE_LOG_CATEGORY_EXTERN(LogMMD4UE4_PmxMeshInfo, Log, All)

	////////////////////////////////////////////////////////////////////
	// Inport用 meta data 格納クラス
	class PmxMeshInfo : public MMDImportHelper
	{
		//////////////////////////////////////
		// Sort Parent Bone ( sort tree bones)
		// memo: ボーンの配列で子->親の順の場合、
		//       ProcessImportMeshSkeleton内部のcheckに引っかかりクラッシュするため。
		// how to: after PMXLoaderBinary func.
		//////////////////////////////////////
		bool FixSortParentBoneIndex();

		TArray<PMX_BONE_HIERARCHY>	fixedHierarchyBone;

	public:
		PmxMeshInfo();
		~PmxMeshInfo();

		///////////////////////////////////////
		bool PMXLoaderBinary(
			const uint8 *& Buffer,
			const uint8 * BufferEnd
			);
		
		///////////////////////////////////////

		char				magic[4];
		float				formatVer;
		PMX_BASEINFO		baseHeader;
		FString				modelNameJP;
		FString				modelNameEng;
		FString				modelCommentJP;
		FString				modelCommentEng;
		//
		TArray<PMX_VERTEX>	vertexList;
		TArray<PMX_FACE>	faceList;

		TArray<PMX_TEXTURE>	textureList;
		TArray<PMX_MATERIAL>	materialList;

		TArray<PMX_BONE>	boneList;
		TArray<PMX_MORPH>	morphList;
	};

}

///////////////////////////////////////////////////////////////////
//	Compy Refafct FBImporter.h
/////////////////////////////////////////////

struct PMXImportOptions
{
	// General options
	bool bImportMaterials;
	bool bInvertNormalMap;
	bool bImportTextures;
	bool bCreateMaterialInstMode;
	bool bUnlitMaterials;
	bool bImportLOD;
	bool bUsedAsFullName;
	bool bConvertScene;
	bool bRemoveNameSpace;
	FVector ImportTranslation;
	FRotator ImportRotation;
	float ImportUniformScale;
	EMMDNormalImportMethod NormalImportMethod;
	// Static Mesh options
	bool bCombineToSingle;
	EVertexColorImportOptionMMD::Type VertexColorImportOption;
	FColor VertexOverrideColor;
	bool bRemoveDegenerates;
	bool bGenerateLightmapUVs;
	bool bOneConvexHullPerUCX;
	bool bAutoGenerateCollision;
	FName StaticMeshLODGroup;
	// Skeletal Mesh options
	bool bImportMorph;
	bool bImportAnimations;
	bool bUpdateSkeletonReferencePose;
	bool bResample;
	bool bImportRigidMesh;
	bool bUseT0AsRefPose;
	bool bPreserveSmoothingGroups;
	bool bKeepOverlappingVertices;
	bool bImportMeshesInBoneHierarchy;
	bool bCreatePhysicsAsset;
	UPhysicsAsset *PhysicsAsset;
	// Animation option
	USkeleton* SkeletonForAnimation;
	//EFBXAnimationLengthImportType AnimationLengthImportType;
	struct FIntPoint AnimationRange;
	FString AnimationName;
	bool	bPreserveLocalTransform;
	bool	bImportCustomAttribute;

	UAnimSequence* AnimSequenceAsset;
	UDataTable* MMD2UE4NameTableRow;
	UMMDExtendAsset* MmdExtendAsset;
};

PMXImportOptions* GetImportOptions(
	class FPmxImporter* PmxImporter,
	UPmxImportUI* ImportUI,
	bool bShowOptionDialog,
	const FString& FullPath,
	bool& bOutOperationCanceled,
	bool& bOutImportAll,
	bool bIsObjFormat,
	bool bForceImportType = false,
	EPMXImportType ImportType = PMXIT_StaticMesh
	);

void ApplyImportUIToImportOptions(
	UPmxImportUI* ImportUI,
	PMXImportOptions& InOutImportOptions
	);

/**
* Main PMX Importer class.
*/
class FPmxImporter
{
public:
	~FPmxImporter();
	/**
	* Returns the importer singleton. It will be created on the first request.
	*/
	IM4U_API static FPmxImporter* GetInstance();
	static void DeleteInstance();

	/**
	* Get the object of import options
	*
	* @return FBXImportOptions
	*/
	IM4U_API PMXImportOptions* GetImportOptions() const;

public:
	PMXImportOptions* ImportOptions;
protected:
	static TSharedPtr<FPmxImporter> StaticInstance;

	FPmxImporter();

	/**
	* Clean up for destroy the Importer.
	*/
	void CleanUp();
};
