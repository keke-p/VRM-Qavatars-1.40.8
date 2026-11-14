#include "AssetLib/generators/springBoneGenerator.hpp"

#include "customTypes/VRMSpringBoneColliderGroup.hpp"
#include "customTypes/VRMSpringBone.hpp"

namespace AssetLib::Generators
{
    Sombrero::FastVector3 convertVector(const VRMC_VRM_0_0::Vector3 vec)
    {
        return {vec.x, vec.y, vec.z};
    }

    // the nodes are offset by one, the first one is the scene root added by assimp
    UnityEngine::GameObject* getNodeObject(const AssetLib::Structure::VRM::VRMModelContext* modelContext, const int64_t gltfNode)
    {
        const int64_t index = gltfNode + 1;
        if (index < 0 || index >= static_cast<int64_t>(modelContext->nodes.size()))
            return nullptr;
        return modelContext->nodes[index]->gameObject;
    }

    template<>
    void SpringBoneGenerator::Generate(const VRMC_VRM_0_0::Vrm& vrm, AssetLib::Structure::VRM::VRMModelContext* modelContext, UnityEngine::GameObject* secondary)
    {
        // same order as in the file, the bone groups refer to the collider groups by index
        auto colliders = std::vector<VRMQavatars::VRMSpringBoneColliderGroup*>();
        for (const auto& colliderGroup : vrm.secondaryAnimation.colliderGroups)
        {
            if (auto node = getNodeObject(modelContext, colliderGroup.node))
            {
                auto vrmGroup = node->AddComponent<VRMQavatars::VRMSpringBoneColliderGroup*>();
                for (auto colliderRef : colliderGroup.colliders)
                {
                    vrmGroup->colliders.emplace_back(convertVector(colliderRef.offset), colliderRef.radius);
                }
                colliders.push_back(vrmGroup);
            }
            else
            {
                colliders.push_back(nullptr);
            }
        }

        auto springs = vrm.secondaryAnimation.boneGroups;
        for (auto springRef : springs)
        {
             auto springBone = secondary->get_gameObject()->AddComponent<VRMQavatars::VRMSpringBone*>();

            if (springRef.center != -1)
            {
                if (auto center = getNodeObject(modelContext, springRef.center))
                    springBone->center = center->get_transform();
            }

            springBone->comment = springRef.comment;
            springBone->dragForce = springRef.dragForce;
            springBone->gravityDir = convertVector(springRef.gravityDir);
            springBone->gravityPower = springRef.gravityPower;
            springBone->hitRadius = springRef.hitRadius;
            springBone->stiffnessForce = springRef.stiffiness;
            if ( springRef.colliderGroups.size() > 0)
            {
                springBone->colliderGroups = std::vector<VRMQavatars::VRMSpringBoneColliderGroup*>(springRef.colliderGroups.size());
                for (int j = 0; j < springRef.colliderGroups.size(); j++)
                {
                    auto group = springRef.colliderGroups[j];
                    springBone->colliderGroups[j] = group >= 0 && group < colliders.size() ? colliders[group] : nullptr;
                }
            }
            auto list2 = ArrayW<UnityEngine::Transform*>(springRef.bones.size());
            for (size_t k = 0; k < springRef.bones.size(); k++)
            {
                auto bone = springRef.bones[k];
                const auto boneObject = getNodeObject(modelContext, bone);
                list2[k] = boneObject ? boneObject->get_transform() : nullptr;
            }
            springBone->rootBones = list2;
        }   
    }

    template<>
    void SpringBoneGenerator::Generate(const VRMC_VRM_1_0::Vrm& vrm, AssetLib::Structure::VRM::VRMModelContext* modelContext, UnityEngine::GameObject* secondary)
    {
    }
}