#include "customTypes/BlendShape/BlendShapeController.hpp"

#include <GlobalNamespace/OVRInput.hpp>
#include <UnityEngine/Time.hpp>
#include <UnityEngine/WaitForSeconds.hpp>

#include "chatplex-sdk-bs/shared/CP_SDK/ChatPlexSDK.hpp"

#include "AvatarManager.hpp"

#include "UnityEngine/Animator.hpp"
#include "UnityEngine/HumanBodyBones.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/SkinnedMeshRenderer.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/Bounds.hpp"
#include "UnityEngine/Random.hpp"
#include "UnityEngine/Mathf.hpp"

#include "GlobalNamespace/OVRInput.hpp"

#include "chatplex-sdk-bs/shared/CP_SDK/Utils/Il2cpp.hpp"
#include "main.hpp"
#include "config/ConfigManager.hpp"

namespace VRMQavatars::BlendShape
{
    CP_SDK_IL2CPP_INHERIT_INIT(BlendShapeController);

    CP_SDK_IL2CPP_DECLARE_CTOR_IMPL(BlendShapeController)
    {

    }

    CP_SDK_IL2CPP_DECLARE_DTOR_MONOBEHAVIOUR_IMPL(BlendShapeController)
    {

    }

    custom_types::Helpers::Coroutine BlendShapeController::AutoBlinkCoroutine()
    {
        while(true)
        {
            constexpr float minTime = 1.5f;
            auto config = Config::ConfigManager::GetBlendshapeSettings();
            const float toWait = UnityEngine::Time::get_time() + minTime + (UnityEngine::Random::get_value() * config.waitTime);
            while(UnityEngine::Time::get_time() < toWait)
            {
                co_yield nullptr;
            }
            config = Config::ConfigManager::GetBlendshapeSettings();
            // not every avatar has a blink
            const auto blink = blendShapePresetMappings.find(AssetLib::Structure::VRM::Blink);
            if(config.autoBlink && allowAutoBlink && blink != blendShapePresetMappings.end())
            {
                const int blinkIdx = blink->second.idx;
                float blinkValue = 0.0f;

                constexpr float closeSpeed = 9.0f;
                constexpr float openSpeed = 9.0f;

                constexpr float closeDuration = 0.15f;
                //Animate closing
                while(true)
                {
                    blinkValue += UnityEngine::Time::get_deltaTime() * closeSpeed;
                    if (blinkValue >= 1.0f || !allowAutoBlink)
                    {
                        break;
                    }
                    blendShapeValues[blinkIdx] = blinkValue;
                    co_yield nullptr;
                }
                //Ensure full blink
                blinkValue = 1.0f;
                blendShapeValues[blinkIdx] = blinkValue;

                //Wait for duration of blink
                co_yield reinterpret_cast<System::Collections::IEnumerator*>(UnityEngine::WaitForSeconds::New_ctor(closeDuration));

                //Animate Opening
                while(true)
                {
                    blinkValue -= UnityEngine::Time::get_deltaTime() * openSpeed;
                    if (blinkValue < 0.0f || !allowAutoBlink)
                    {
                        break;
                    }
                    blendShapeValues[blinkIdx] = blinkValue;
                    co_yield nullptr;
                }
                //Ensure no more blinking
                blinkValue = 0.0f;
                blendShapeValues[blinkIdx] = blinkValue;
            }
        }
        co_return;
    }

    void BlendShapeController::Init()
    {
        init = false;
        const auto animator = GetComponent<UnityEngine::Animator*>();

        leftEye = animator->GetBoneTransform(UnityEngine::HumanBodyBones::LeftEye);
        rightEye = animator->GetBoneTransform(UnityEngine::HumanBodyBones::RightEye);

        const auto nodes = AvatarManager::currentContext->nodes;
        for (int i = 0; i < nodes.size(); ++i)
        {
            const auto node = nodes[i];
            if(node->mesh.has_value())
            {
                renderers.push_back(node->gameObject->GetComponent<UnityEngine::SkinnedMeshRenderer*>());
            }
        }

        const auto blendShapeMaster = AvatarManager::currentContext->blendShapeMaster;
        for (auto blendShape : blendShapeMaster->groups)
        {
            blendShapePresetMappings[blendShape.preset] = blendShape;
            blendShapeMappings[blendShape.idx] = blendShape;
            blendShapeValues[blendShape.idx] = 0.0f;
            blendShapeTargetValues[blendShape.idx] = 0.0f;
            reverseMappings[blendShape.name] = blendShape.idx;
        }
        init = true;

        StartCoroutine(custom_types::Helpers::CoroutineHelper::New(AutoBlinkCoroutine()));
    }

    const std::vector<GlobalNamespace::OVRInput::Button> buttons = {
        GlobalNamespace::OVRInput::Button::None,
        GlobalNamespace::OVRInput::Button::PrimaryHandTrigger,
        GlobalNamespace::OVRInput::Button::PrimaryIndexTrigger,
        GlobalNamespace::OVRInput::Button::One,
        GlobalNamespace::OVRInput::Button::Two,
        GlobalNamespace::OVRInput::Button::PrimaryThumbstickUp,
        GlobalNamespace::OVRInput::Button::PrimaryThumbstickDown,
        GlobalNamespace::OVRInput::Button::PrimaryThumbstickLeft,
        GlobalNamespace::OVRInput::Button::PrimaryThumbstickRight
    };
    const std::vector<GlobalNamespace::OVRInput::Controller> controllers = {
        GlobalNamespace::OVRInput::Controller::LTouch,
        GlobalNamespace::OVRInput::Controller::RTouch
    };

    inline bool IsButtonDown(const int& buttonIdx, const int controller) {
        if(buttonIdx <= 0) return false;
        const auto button = buttons[buttonIdx];
        if(controller == 2)
            return GlobalNamespace::OVRInput::Get(button, controllers[0]) || GlobalNamespace::OVRInput::Get(button, controllers[1]);
        return GlobalNamespace::OVRInput::Get(button, controllers[controller]);
    }

    bool SkipBlendShape(const AssetLib::Structure::VRM::BlendShapePreset preset, const BlendshapeSettings& settings, bool allow)
    {
        if(settings.autoBlink && allow)
        {
            return preset == AssetLib::Structure::VRM::Blink;
        }
        return false;
    }

    void BlendShapeController::Update()
    {
        if(!init) return;
        bool any = false; //Used to determine if we should lerp to neutral preset
        auto triggerConfig = Config::ConfigManager::GetControllerTriggerSettings();
        auto blendShapeConfig = Config::ConfigManager::GetBlendshapeSettings();

        //Reset All to 0
        for (auto const& [key, val] : blendShapeTargetValues)
        {
            blendShapeTargetValues[key] = 0.0f;
        }

        //Check for inputs
        // an expression this avatar doesn't have is ignored
        const auto press = [&](const std::string& expression, const int button, const int controller)
        {
            if(expression == "None" || !IsButtonDown(button, controller)) return;
            const auto mapping = reverseMappings.find(expression);
            if(mapping == reverseMappings.end()) return;
            any = true; //Used to keep the neutral preset out of the way
            blendShapeTargetValues[mapping->second] = 1.0f;
        };

        press(triggerConfig.A, 3, 1);
        press(triggerConfig.B, 4, 1);
        press(triggerConfig.X, 3, 0);
        press(triggerConfig.Y, 4, 0);
        press(triggerConfig.LGrip, 1, 0);
        press(triggerConfig.RGrip, 1, 1);
        press(triggerConfig.LTrigger, 2, 0);
        press(triggerConfig.RTrigger, 2, 1);

        if(!any)
        {
            const auto neutral = reverseMappings.find(blendShapeConfig.neutralExpression);
            if(neutral != reverseMappings.end())
                blendShapeTargetValues[neutral->second] = 1.0f;
        }

        allowAutoBlink = !any;

        //Lerp Values

        for (auto const& [key, val] : blendShapeTargetValues)
        {
            if(SkipBlendShape(blendShapeMappings[key].preset, blendShapeConfig, allowAutoBlink) && !any) continue;
            blendShapeValues[key] = UnityEngine::Mathf::Lerp(blendShapeValues[key], val, UnityEngine::Time::get_deltaTime() * 10.0f);
        }

        //Set Values

        for (auto const& [key, val] : blendShapeValues)
        {
            SetBlendshape(key, val * 100.0f);
        }

        if(blendShapeConfig.mockEyeMovement)
        {
            if(UnityEngine::Time::get_time() > time)
            {
                //Switch eye positions
                Sombrero::FastVector3 unitPos = Sombrero::FastVector3(UnityEngine::Random::get_onUnitSphere()) * std::cbrt(UnityEngine::Random::get_value()); //random lookdir (insideUnitSphere is stripped)
                eyeVec.x = unitPos.x * 2.5f; //vert
                eyeVec.y = unitPos.y * 5.0f; //horz
                eyeVec.z = 0.0f;

                time = UnityEngine::Time::get_time() + UnityEngine::Random::Range(0.75f, 2.5f);
            }
        }
        else
        {
            eyeVec = Sombrero::FastVector3::zero();
        }

        if(leftEye != nullptr && rightEye != nullptr)
        {
            auto leftAngle = UnityEngine::Quaternion::Lerp(leftEye->get_localRotation(), UnityEngine::Quaternion::Euler(eyeVec), UnityEngine::Time::get_deltaTime() * 15.0f);
            auto rightAngle = UnityEngine::Quaternion::Lerp(rightEye->get_localRotation(), UnityEngine::Quaternion::Euler(eyeVec), UnityEngine::Time::get_deltaTime() * 15.0f);
            leftEye->set_localRotation(leftAngle);
            rightEye->set_localRotation(rightAngle);
        }
    }

    void BlendShapeController::OnDestroy()
    {

    }

    float smoothstep(float edge0, float edge1, float x, float max) {
        // Scale, bias and saturate x to 0..1 range
        x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, max);
        // Evaluate polynomial
        return x * x * (3 - 2 * x);
    }

    void BlendShapeController::SetBlendshape(const int preset, const float value)
    {
        const auto mapping = blendShapeMappings.find(preset);
        if(mapping == blendShapeMappings.end()) return;
        const auto& group = mapping->second;
        for (const auto& bind : group.binds)
        {
            if(bind.mesh < 0 || bind.mesh >= renderers.size()) continue;
            const auto renderer = renderers[bind.mesh];
            if(renderer->get_sharedMesh()->get_blendShapeCount() > bind.index)
            {
                const auto weight = (bind.weight / 100.0f);
                const float newValue = group.isBinary ? smoothstep(5.0f, 10.0f, value, weight) : value * weight;
                renderer->SetBlendShapeWeight(bind.index, newValue);
            }
        }
    }
};