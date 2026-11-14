#pragma once
#include <vector>
#include <optional>

#include "node.hpp"
#include "armature.hpp"
#include "assimp/shared/assimp/scene.h"
#include "UnityEngine/GameObject.hpp"

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