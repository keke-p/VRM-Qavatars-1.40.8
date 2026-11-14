#pragma once

#include "chatplex-sdk-bs/shared/CP_SDK/UI/Data/IListItem.hpp"

#include <TMPro/TextAlignmentOptions.hpp>

#include "vrm/VRMDescriptor.hpp"

#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture2D.hpp"

namespace VRMQavatars::UI::Components {

    class AvatarListItem : public CP_SDK::UI::Data::IListItem
    {
        CP_SDK_NO_DEF_CTORS(AvatarListItem);

        public:
            VRMDescriptor descriptor;

            // the item owns its thumbnail and the sprite made from it, both go away with the item
            SafePtrUnity<UnityEngine::Texture2D> thumbnail;
            SafePtrUnity<UnityEngine::Sprite> sprite;

            AvatarListItem(const VRMDescriptor& descriptor);
            virtual ~AvatarListItem();

            virtual void OnShow() override;
    };
}
