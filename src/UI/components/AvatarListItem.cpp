#include "UI/components/AvatarListItem.hpp"

#include "bsml/shared/Helpers/utilities.hpp"

#include "assets.hpp"
#include "UI/components/AvatarListCell.hpp"

#include "UnityEngine/SpriteMeshType.hpp"

namespace VRMQavatars::UI::Components {

    AvatarListItem::AvatarListItem(const VRMDescriptor& descriptor)
    {
        this->descriptor = descriptor;
        if(descriptor.thumbnail != nullptr)
            thumbnail = descriptor.thumbnail;
    }

    AvatarListItem::~AvatarListItem()
    {
        // the list is rebuilt on every refresh
        if(sprite)
            UnityEngine::Object::Destroy(sprite.ptr());
        if(thumbnail)
            UnityEngine::Object::Destroy(thumbnail.ptr());
    }

    void AvatarListItem::OnShow()
    {
        static auto cdsprite = BSML::Utilities::LoadSpriteRaw(Assets::cd_png);
        const auto l_TextListCell = GetCellAsClassOf<AvatarListCell*>();
        if (!l_TextListCell)
            return;

        l_TextListCell->NameText->Element()->TMProUGUI()->set_text(descriptor.name);
        l_TextListCell->AuthorText->Element()->TMProUGUI()->set_text(descriptor.author);
        l_TextListCell->FilenameText->Element()->TMProUGUI()->set_text(descriptor.filePath);
        if(thumbnail)
        {
            // showing happens every time the cell scrolls into view, make the sprite once
            if(!sprite)
            {
                const auto texture = thumbnail.ptr();
                sprite = UnityEngine::Sprite::Create(texture, UnityEngine::Rect(0.0f, 0.0f, static_cast<float>(texture->get_width()), static_cast<float>(texture->get_height())), UnityEngine::Vector2(0.5f, 0.5f), 1024.0f, 1u, UnityEngine::SpriteMeshType::FullRect, UnityEngine::Vector4(0.0f, 0.0f, 0.0f, 0.0f), false);
            }
            l_TextListCell->Image->Element()->SetSprite(sprite.ptr());
        }
        else
        {
            l_TextListCell->Image->Element()->SetSprite(cdsprite);
        }
    }
}
