#pragma once

#include <chatplex-sdk-bs/shared/CP_SDK/Utils/Delegate.hpp>
#include <chatplex-sdk-bs/shared/CP_SDK/Utils/Event.hpp>

#include "main.hpp"
#include "AssetLib/structure/VRM/VRMmodelContext.hpp"

#include "customTypes/TargetManager.hpp"

#include "utils/structs/OffsetPose.hpp"

#include "customTypes/FinalIK/VRIK.hpp"
#include "customTypes/FinalIK/IKSolverVR.hpp"

namespace VRMQavatars {
    class AvatarManager
    {
        public:
        static void StartupLoad();

        static AssetLib::Structure::VRM::VRMModelContext* currentContext;
        static void SetContext(AssetLib::Structure::VRM::VRMModelContext* context);

        // counts the avatar selections (including none): loading takes a while, and a load that finishes
        // after something else was selected must not replace that selection
        static inline int selection = 0;
        // removes a loaded avatar that isn't going to be used
        static void DiscardContext(AssetLib::Structure::VRM::VRMModelContext* context);

        static void SetHandOffset(const Structs::OffsetPose& pose);

        static void UpdateVRIK();

        static void SetFingerPose(const std::string& value);

        static CP_SDK::Utils::Event<> OnLoad;

        public:
        static FinalIK::VRIK* vrik;
        static TargetManager* targetManager;
    };
};