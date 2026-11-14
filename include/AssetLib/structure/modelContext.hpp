#pragma once
#include <vector>
#include <optional>

#include "node.hpp"
#include "armature.hpp"
#include "assimp/shared/assimp/scene.h"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"

namespace AssetLib::Structure
{
    //Contains all information used during loading
    class ModelContext
    {
        public:
        std::string fileName;
        const aiScene* originalScene;

        std::vector<Node*> nodes;
        Node* rootNode;

        bool isSkinned;
        std::optional<Armature> armature;

        UnityEngine::GameObject* rootGameObject;

        // The meshes, materials, textures etc. created for this model. Unity doesn't free those along with the
        // objects that use them, so they are destroyed together with the model.
        std::vector<SafePtrUnity<UnityEngine::Object>> assets;

        void Own(UnityEngine::Object* asset)
        {
            if(asset != nullptr)
                assets.emplace_back(asset);
        }

        // Destroys the model's objects in the scene and its assets. Main thread only.
        void DestroyUnityObjects()
        {
            if(rootGameObject != nullptr)
                UnityEngine::Object::Destroy(rootGameObject);
            rootGameObject = nullptr;

            for (auto& asset : assets)
            {
                if(asset)
                    UnityEngine::Object::Destroy(asset.ptr());
            }
            assets.clear();
        }

        ModelContext() = default;
        // the nodes belong to the context (a moved-from context has none left)
        virtual ~ModelContext()
        {
            for (const auto node : nodes)
                delete node;
        }
        ModelContext(ModelContext&&) = default;
    };
};