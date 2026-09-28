#include "Core/MTEditorScriptingLibrary.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ReferenceSkeleton.h"

bool UMTEditorScriptingLibrary::AddOrUpdateSkeletalMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName, float Offset,
	FName TowardChildBone)
{
#if WITH_EDITOR
	if (!Mesh || SocketName.IsNone())
	{
		return false;
	}
	const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return false;
	}
	// Direction in the bone's own space: the child's reference-pose translation (bone axes need not follow the limb).
	FVector Location = FVector::ZeroVector;
	const int32 ChildIndex = TowardChildBone.IsNone() ? INDEX_NONE : RefSkeleton.FindBoneIndex(TowardChildBone);
	if (ChildIndex != INDEX_NONE && RefSkeleton.GetParentIndex(ChildIndex) == BoneIndex)
	{
		Location = RefSkeleton.GetRefBonePose()[ChildIndex].GetTranslation().GetSafeNormal() * Offset;
	}
	else if (!TowardChildBone.IsNone())
	{
		return false;
	}

	Mesh->Modify();
	USkeletalMeshSocket* Socket = Mesh->FindSocket(SocketName);
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Mesh);
		Socket->SocketName = SocketName;
		Mesh->AddSocket(Socket); // mesh socket: every lineage has its own skeleton asset
	}
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = Location;
	Socket->RelativeRotation = FRotator::ZeroRotator;
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
