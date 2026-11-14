#pragma once

#include "AssetLib/VRMConcepts.hpp"

#include "UnityEngine/Material.hpp"
#include "UnityEngine/Texture2D.hpp"

#include <vector>

namespace AssetLib::Generators
{
    class MaterialGenerator
    {
        public:
        MaterialGenerator() = default;
        ~MaterialGenerator() = default;

        // textures: indexed like the glTF textures, which is what the material properties refer to
        template<AssetLib::VRMVersion T>
        UnityEngine::Material* Generate(const T& vrm, int index, const std::vector<UnityEngine::Texture2D*>& textures);
    };
};