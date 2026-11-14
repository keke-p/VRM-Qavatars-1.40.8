#include "MaterialTracker.hpp"

#include "main.hpp"
#include "MirrorManager.hpp"

namespace VRMQavatars
{
    bool MaterialTracker::bloomEnabled = false;
    std::vector<SafePtrUnity<UnityEngine::Material>> MaterialTracker::materials;

    void MaterialTracker::UpdateMaterials()
    {
        for(auto& material : materials)
        {
            if(material)
            {
                material->SetInt("_BloomSupport", bloomEnabled ? 1 : 0);
                material->SetInt("_ColorMask", bloomEnabled ? 14 : 15);
            }
        }
        if(MirrorManager::mainMirror)
        {
            MirrorManager::UpdateMirror(false);
        }
    }
}
