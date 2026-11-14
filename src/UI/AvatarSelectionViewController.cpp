#include "UI/AvatarSelectionViewController.hpp"

#include <string_view>
#include "UnityEngine/WaitForEndOfFrame.hpp"
#include "UnityEngine/TextureFormat.hpp"

#include "AvatarManager.hpp"
#include "assets.hpp"
#include "CalibrationHelper.hpp"
#include "TPoseHelper.hpp"
#include "AssetLib/modelImporter.hpp"
#include "AssetLib/gLTFImageReader.hpp"
#include "chatplex-sdk-bs/shared/CP_SDK/XUI/Templates.hpp"
#include "config/ConfigManager.hpp"
#include "UI/components/AvatarListCell.hpp"
#include "UI/components/AvatarListItem.hpp"

#include "utils/FileUtils.hpp"

#include "bsml/shared/BSML/MainThreadScheduler.hpp"

namespace VRMQavatars::UI::ViewControllers {
    CP_SDK_IL2CPP_INHERIT_INIT(AvatarSelectionViewController);

    CP_SDK_IL2CPP_DECLARE_CTOR_IMPL(AvatarSelectionViewController)
    {
        OnViewCreation      = {this, &AvatarSelectionViewController::DidActivate};
    }

    CP_SDK_IL2CPP_DECLARE_DTOR_MONOBEHAVIOUR_IMPL(AvatarSelectionViewController)
    {

    }

    bool is_whitespace(const std::string& s) {
        return std::all_of(s.begin(), s.end(), isspace);
    }

    VRMDescriptor GetNoneDescriptor()
    {
        VRMDescriptor descriptor;
        descriptor.name = "None";
        descriptor.author = "No avatar";

        UnityEngine::Texture2D* texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
        UnityEngine::ImageConversion::LoadImage(texture, Assets::none_png, false);

        descriptor.thumbnail = texture;

        descriptor.filePath = "";

        return descriptor;
    }

    std::ifstream::pos_type filesize(const char* filename)
    {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg();
    }

    // throws when the file isn't a VRM this mod can load
    VRMDescriptor ReadVRMDescriptor(const std::string& path)
    {
        VRMDescriptor descriptor;
        const std::string fullPath = vrm_path + std::string("/") + path;

        // 12 byte glb header, 8 byte chunk header, then the json
        const auto size = static_cast<int64_t>(filesize(fullPath.c_str()));
        if(size < 20)
            throw std::runtime_error("not a glb file");

        //Load in binary to parse out VRM data
        std::ifstream binFile(fullPath, std::ios::binary);
        binFile.seekg(12); //Skip past the 12 byte header, to the json header
        uint32_t jsonLength = 0;
        binFile.read(reinterpret_cast<char*>(&jsonLength), sizeof(uint32_t)); //Read the length of the json file from it's header
        if(!binFile || jsonLength > size - 20)
            throw std::runtime_error("invalid json chunk length");

        std::string jsonStr;
        jsonStr.resize(jsonLength);
        binFile.seekg(20); // Skip the rest of the JSON header to the start of the string
        binFile.read(jsonStr.data(), jsonLength); // Read out the json string

        const auto doc = nlohmann::json::parse(jsonStr);
        if(!doc.contains("extensions") || !doc["extensions"].contains("VRM"))
        {
            // VRM 1.0 materials and spring bones are not implemented
            throw std::runtime_error(doc.contains("extensions") && doc["extensions"].contains("VRMC_vrm") ? "VRM 1.0 is not supported" : "no VRM data");
        }

        VRMC_VRM_0_0::Vrm vrm;
        from_json(doc["extensions"]["VRM"], vrm);
        descriptor.vrm0 = vrm;

        descriptor.filePath = path;
        descriptor.name = is_whitespace(vrm.meta.title) ? path : vrm.meta.title;
        descriptor.author = vrm.meta.author;

        try
        {
            descriptor.thumbnail = AssetLib::gLTFImageReader::ReadImageIndex(doc, jsonLength, binFile, vrm.meta.texture);
        }
        catch (const std::exception& ex)
        {
            VRMLogger.error("Couldn't read the thumbnail of {}: {}", path, ex.what());
            descriptor.thumbnail = nullptr;
        }

        return descriptor;
    }

    VRMDescriptor LoadVRMDescriptor(const std::string& path) {
        try
        {
            return ReadVRMDescriptor(path);
        }
        catch (const std::exception& ex)
        {
            // one bad file must not take the whole list down
            VRMLogger.error("Skipping {}: {}", path, ex.what());
            VRMDescriptor descriptor;
            descriptor.valid = false;
            return descriptor;
        }
    }

    void AvatarSelectionViewController::Refresh()
    {
        std::vector<std::string> vrms = {};
        if(GetFilesInFolderPath("vrm", vrm_path, vrms))
        {
            std::vector<Components::AvatarListItem::Ptr> avatarSet;

            avatarSet.push_back(std::make_shared<Components::AvatarListItem>(GetNoneDescriptor()));
            for(auto& vrm : vrms)
            {
                auto descriptor = LoadVRMDescriptor(vrm);
                if(descriptor.valid)
                {
                    avatarSet.push_back(std::make_shared<Components::AvatarListItem>(descriptor));
                }
            }
            avatarList->SetListItems(avatarSet);
            auto& globcon = Config::ConfigManager::GetGlobalConfig();
            if(globcon.hasSelected.GetValue())
            {
                auto pred = [&globcon](const std::shared_ptr<CP_SDK::UI::Data::IListItem>& ptr) { return reinterpret_cast<const Components::AvatarListItem*>(ptr.get())->descriptor.filePath == globcon.selectedFileName.GetValue(); };
                const auto it = std::find_if(std::begin(avatarSet),
                                             std::end(avatarSet),
                                             pred);
                // the selected file may have been removed
                if(it != std::end(avatarSet))
                    avatarList->SetSelectedListItem(*it, false);
            }
        }
    }

    void AvatarSelectionViewController::OnListSelect(const Components::AvatarListItem::Ptr& p_ListItem)
    {
        auto& globcon = Config::ConfigManager::GetGlobalConfig();
        const auto item = *reinterpret_cast<const std::shared_ptr<Components::AvatarListItem>*>(&p_ListItem);
        if (item)
        {
            const auto path = item->descriptor.filePath;
            if(!item->descriptor.valid) return;
            if(path == "")
            {
                ++AvatarManager::selection;
                Config::ConfigManager::ResetAvatarConfig(false);
                AvatarManager::SetContext(nullptr);
                globcon.hasSelected.SetValue(false);
                globcon.selectedFileName.SetValue(path);
                return;
            }
            if(fileexists(std::string(vrm_path) + "/" + path))
            {
                const int selected = ++AvatarManager::selection;
                std::shared_future<AssetLib::Structure::VRM::VRMModelContext*> future = AssetLib::ModelImporter::LoadVRM(std::string(vrm_path) + "/" + path);
                BSML::MainThreadScheduler::AwaitFuture(future, [path, item, future, selected, this](){
                    AssetLib::Structure::VRM::VRMModelContext* ctx = future.get();
                    // something else was selected while this one was loading
                    if(selected != AvatarManager::selection)
                    {
                        AvatarManager::DiscardContext(ctx);
                        return;
                    }

                    // the values the new avatar's config doesn't have start from the defaults, not from the previous
                    // avatar's. Only reset them now: until here the previous avatar was still in use, and a setting
                    // changed meanwhile would have saved the defaults to its config
                    Config::ConfigManager::ResetAvatarConfig(false);
                    AvatarManager::SetContext(ctx);

                    auto& globcon = Config::ConfigManager::GetGlobalConfig();

                    // the avatar couldn't be loaded
                    if(ctx == nullptr)
                    {
                        globcon.hasSelected.SetValue(false);
                        return;
                    }

                    globcon.hasSelected.SetValue(true);
                    globcon.selectedFileName.SetValue(path);

                    auto& avacon = Config::ConfigManager::GetAvatarConfig();
                    if(!avacon.HasAgreedToTerms.GetValue())
                    {
                        agreementModal->SetInfo(item->descriptor);
                        ShowModal(agreementModal.Ptr());
                    }
                    if(avacon.HasCalibrated.GetValue())
                    {
                        CalibrationHelper::Calibrate(avacon.CalibratedScale.GetValue());
                    }
                });
            }
        }
    }

    void AvatarSelectionViewController::Calibrate()
    {
        CalibrationHelper::StartCalibration();
    }

    void AvatarSelectionViewController::DidActivate()
    {
        agreementModal = CreateModal<Modals::AvatarSelectionModal>();

        CP_SDK::XUI::Templates::FullRectLayoutMainView({
            CP_SDK::XUI::XUIVLayout::Make({
                CP_SDK::XUI::XUIHLayout::Make({
                    CP_SDK::XUI::XUISecondaryButton::Make(u"Refresh")
                        ->OnClick({this, &AvatarSelectionViewController::Refresh})
                        ->AsShared(),
                    CP_SDK::XUI::XUIPrimaryButton::Make(u"Recalibrate")
                        ->OnClick({this, &AvatarSelectionViewController::Calibrate})
                        ->SetInteractable(AvatarManager::currentContext != nullptr)
                        ->Bind(&RecalibrateButton)
                        ->AsShared(),
                })
                ->SetSpacing(8.0f)
                ->AsShared(),
                CP_SDK::XUI::XUIHLayout::Make({
                    CP_SDK::XUI::XUIVVList::Make()
                        ->SetListCellPrefab(CP_SDK::UI::Data::ListCellPrefabs<Components::AvatarListCell>::Get())
                        ->OnListItemSelected({this, &AvatarSelectionViewController::OnListSelect})
                        ->Bind(&avatarList)
                        ->AsShared()
                })
                ->SetHeight(58)
                ->SetSpacing(0)
                ->SetPadding(0)
                ->OnReady([](CP_SDK::UI::Components::CHLayout* x) -> void {
                    x->CSizeFitter()->set_horizontalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
                    x->CSizeFitter()->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::Unconstrained);
                    x->HOrVLayoutGroup()->set_childForceExpandWidth(true);
                    x->HOrVLayoutGroup()->set_childForceExpandHeight(true);
                    x->LElement()->set_preferredWidth(80);
                })
                ->AsShared(),
            })
            ->SetSpacing(3.0f)
            ->AsShared()
        })
        ->BuildUI(get_transform());

        Refresh();

        AvatarManager::OnLoad += [this]
        {
            RecalibrateButton->SetInteractable(AvatarManager::currentContext != nullptr);
        };
    }
}
