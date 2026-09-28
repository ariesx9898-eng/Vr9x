// Editor-scripting helpers for the Content/Python setup scripts: asset edits UE's Python API cannot make on its own.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MTEditorScriptingLibrary.generated.h"

class USkeletalMesh;

UCLASS()
class MUSHOKURPG_API UMTEditorScriptingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Adds a socket to a skeletal mesh asset, or updates the one with that name. Python cannot do this by itself
	 * (USkeletalMeshSocket's SocketName and BoneName are read-only to scripts). The socket sits Offset cm from the bone
	 * toward TowardChildBone (a direct child in the reference pose, e.g. the middle finger's base for a palm socket),
	 * or on the bone when TowardChildBone is None. Editor only. Returns false for a missing mesh or bone. The caller
	 * saves the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mushoku|Editor Scripting")
	static bool AddOrUpdateSkeletalMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName, float Offset, FName TowardChildBone);
};
