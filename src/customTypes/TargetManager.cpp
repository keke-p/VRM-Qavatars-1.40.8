#include "customTypes/TargetManager.hpp"

#include <bsml/shared/Helpers/utilities.hpp>

#include "customTypes/GroundOffsetObject.hpp"
#include "customTypes/LegMotionController.hpp"

#include "chatplex-sdk-bs/shared/CP_SDK/ChatPlexSDK.hpp"

#include "SceneEventManager.hpp"
#include "TPoseHelper.hpp"
#include "ReplayCorrection.hpp"

#include "GlobalNamespace/OVRSkeletonRenderer.hpp"
#include "GlobalNamespace/OVRBody.hpp"
#include "GlobalNamespace/OVRPlugin.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Time.hpp"

#include "config/ConfigManager.hpp"

#include "conditional-dependencies/shared/main.hpp"

#include "VMC/VMCClient.hpp"
#include "VMC/VMCServer.hpp"

#include "sombrero/shared/FastQuaternion.hpp"

namespace VRMQavatars
{
    CP_SDK_IL2CPP_INHERIT_INIT(TargetManager);

#ifdef VRMQ_DIAG
    namespace
    {
        // logged once a second in game by debug builds (qpm s build-debug / qmod-debug)
        void LogDiagnostics(TargetManager* self, const Sombrero::FastVector3& headsetPos, const bool inReplay)
        {
            static float lastTime = 0.0f;
            const float now = UnityEngine::Time::get_unscaledTime();
            if(!SceneEventManager::inGame || now - lastTime < 1.0f)
                return;
            lastTime = now;

            auto fmtVec = [](Sombrero::FastVector3 v) { return fmt::format("({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z); };
            const auto refs = self->vrik->references;
            auto root = self->get_transform();
            const Sombrero::FastVector3 headTarget = self->headTarget->transform->position;
            const Sombrero::FastVector3 leftTarget = self->leftHandTarget->transform->position;
            const Sombrero::FastVector3 rightTarget = self->rightHandTarget->transform->position;

            VRMLogger.info("[diag] replay={} noodle={} root={} rootScale={:.3f} headToEyes={}",
                inReplay, self->noodleTrack != nullptr, fmtVec(root->position), root->lossyScale.y, fmtVec(self->headToEyesOffset));
            VRMLogger.info("[diag] headset={} headTarget={} headBone={} headDist={:.3f}",
                fmtVec(headsetPos), fmtVec(headTarget), fmtVec(refs->head->position), UnityEngine::Vector3::Distance(headTarget, refs->head->position));
            VRMLogger.info("[diag] leftTarget={} leftBone={} leftDist={:.3f} rightTarget={} rightBone={} rightDist={:.3f}",
                fmtVec(leftTarget), fmtVec(refs->leftHand->position), UnityEngine::Vector3::Distance(leftTarget, refs->leftHand->position),
                fmtVec(rightTarget), fmtVec(refs->rightHand->position), UnityEngine::Vector3::Distance(rightTarget, refs->rightHand->position));
            VRMLogger.info("[diag] leftFoot={} rightFoot={} pelvis={}",
                fmtVec(refs->leftFoot->position), fmtVec(refs->rightFoot->position), fmtVec(refs->pelvis->position));
        }

        // follows the head target switching between the headset and the replay, e.g. between the songs of a render queue
        void LogSwitch(TargetManager* self, const bool inReplay, UnityEngine::GameObject* camTrans)
        {
            const static auto render = CondDeps::Find<bool>("replay", "IsInRender");
            static bool lastReplay = false, lastRender = false, lastHead = false, lastInGame = false;
            static int verboseFrames = 0;
            static float lastIdleLog = 0.0f;

            const bool inRender = render.has_value() && render.value()();
            const bool replayHead = camTrans != nullptr;
            const int frame = UnityEngine::Time::get_frameCount();
            const float now = UnityEngine::Time::get_unscaledTime();

            auto fmtVec = [](Sombrero::FastVector3 v) { return fmt::format("({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z); };
            auto logPose = [&](const char* label)
            {
                const auto refs = self->vrik->references;
                Sombrero::FastVector3 headsetPos;
                Sombrero::FastQuaternion headsetRot;
                ReplayCorrection::GetTrackedPose(GlobalNamespace::OVRPlugin::Node::Head, headsetPos, headsetRot);
                VRMLogger.info("[diag-switch] {} frame={} dt={:.4f} headset={} replayHead={} headTarget={} headBone={} root={}",
                    label, frame, UnityEngine::Time::get_deltaTime(), fmtVec(headsetPos),
                    replayHead ? fmtVec(camTrans->transform->localPosition) : std::string("none"),
                    fmtVec(self->headTarget->transform->position), fmtVec(refs->head->position), fmtVec(self->get_transform()->position));
                VRMLogger.info("[diag-switch] {} frame={} pelvis={} leftFoot={} rightFoot={} leftTarget={} rightTarget={} saberL={} saberR={}",
                    label, frame, fmtVec(refs->pelvis->position), fmtVec(refs->leftFoot->position), fmtVec(refs->rightFoot->position),
                    fmtVec(self->leftHandTarget->transform->position), fmtVec(self->rightHandTarget->transform->position),
                    fmtVec(self->replayLeftSaberPos), fmtVec(self->replayRightSaberPos));
                if(auto legMotion = self->GetComponent<LegMotionController*>())
                    VRMLogger.info("[diag-switch] {} frame={} jump: hasHeadHeight={} standHeight={:.3f} lastHeadHeight={:.3f} airborne={} jumpLift={:.3f} lowTime={:.2f}",
                        label, frame, legMotion->hasHeadHeight, legMotion->standHeight, legMotion->lastHeadHeight,
                        legMotion->airborne, legMotion->jumpLift, legMotion->lowTime);
            };

            if(inReplay != lastReplay || inRender != lastRender || replayHead != lastHead || SceneEventManager::inGame != lastInGame)
            {
                VRMLogger.info("[diag-switch] state frame={} t={:.3f} replay {}->{} render {}->{} replayHead {}->{} inGame {}->{}",
                    frame, now, lastReplay, inReplay, lastRender, inRender, lastHead, replayHead, lastInGame, SceneEventManager::inGame);
                logPose("change");
                if(replayHead && !lastHead)
                    verboseFrames = 120;
                lastReplay = inReplay;
                lastRender = inRender;
                lastHead = replayHead;
                lastInGame = SceneEventManager::inGame;
            }

            if(verboseFrames > 0)
            {
                verboseFrames--;
                logPose("start");
            }
            else if(!SceneEventManager::inGame || (inReplay && !replayHead))
            {
                // in the menu between songs, or waiting for the replay's head (the targets are not updated meanwhile)
                if(now - lastIdleLog >= 1.0f)
                {
                    lastIdleLog = now;
                    logPose(inReplay ? "waiting" : "menu");
                }
            }
        }
    }
#endif

    CP_SDK_IL2CPP_DECLARE_CTOR_IMPL(TargetManager)
    {

    }

    CP_SDK_IL2CPP_DECLARE_DTOR_MONOBEHAVIOUR_IMPL(TargetManager)
    {

    }

    void TargetManager::Initialize()
    {
        leftHandTarget = UnityEngine::GameObject::New_ctor();
        leftHandTarget->transform->SetParent(transform, false);

        rightHandTarget = UnityEngine::GameObject::New_ctor();
        rightHandTarget->transform->SetParent(transform, false);

        headTarget = UnityEngine::GameObject::New_ctor();
        headTarget->transform->SetParent(transform, false);

        waistTracker = UnityEngine::GameObject::New_ctor();
        waistTracker->transform->SetParent(transform, false);

        chestTracker = UnityEngine::GameObject::New_ctor();
        chestTracker->transform->SetParent(transform, false);

        leftFootTracker = UnityEngine::GameObject::New_ctor();
        leftFootTracker->transform->SetParent(transform, false);

        rightFootTracker = UnityEngine::GameObject::New_ctor();
        rightFootTracker->transform->SetParent(transform, false);

        leftKneeTracker = UnityEngine::GameObject::New_ctor();
        leftKneeTracker->transform->SetParent(transform, false);

        rightKneeTracker = UnityEngine::GameObject::New_ctor();
        rightKneeTracker->transform->SetParent(transform, false);

        vrik->AutoDetectReferences();

        vrik->set_enabled(false);

        gameObject->AddComponent<GroundOffsetObject*>();
    }

    void TargetManager::Update()
    {

        if(!intialized)
            return;

        Sombrero::FastVector3 leftHandPos;
        Sombrero::FastQuaternion leftHandRot;

        Sombrero::FastVector3 rightHandPos;
        Sombrero::FastQuaternion rightHandRot;

        Sombrero::FastVector3 headPos;
        Sombrero::FastQuaternion headRot;

        if(SceneEventManager::inGame && !noodleTrack)
        {
            noodleTrack = UnityEngine::GameObject::Find("NoodlePlayerTrack");
            GetComponent<GroundOffsetObject*>()->set_enabled(false);
        }

        const static auto replay = CondDeps::Find<bool>("replay", "IsInReplay");
        const bool inReplay = replay.has_value() && replay.value()();
        auto camTrans = inReplay ? UnityEngine::GameObject::Find("PlayerTransformsHeadReplacement") : nullptr;
        // the head replacement is created at the origin and gets the replay's pose with the next PlayerTransforms update
        const bool replayHead = camTrans != nullptr && Sombrero::FastVector3(camTrans->transform->localPosition).sqrMagnitude() > 0.0f;
        if(!inReplay)
            replaySabersUpdated = false;
        // the head target jumps when it switches between the headset and the replay, that's not the player jumping
        if(replayHead != usingReplayHead)
        {
            usingReplayHead = replayHead;
            pendingPoseReset = true;
            if(auto legMotion = GetComponent<LegMotionController*>())
                legMotion->ResetJump();
        }
#ifdef VRMQ_DIAG
        LogSwitch(this, inReplay, camTrans);
#endif
        if(inReplay)
        {
            if(!replayHead) return;
            // the head replacement is parented to the tracking space origin, like the real camera
            headPos = camTrans->transform->localPosition;
            headRot = camTrans->transform->localRotation;

            leftHandPos = replayLeftSaberPos;
            leftHandRot = replayLeftSaberRot;

            rightHandPos = replayRightSaberPos;
            rightHandRot = replayRightSaberRot;

            // the replay contains the game's poses, convert them to the raw poses used in live play
            ReplayCorrection::Apply(ReplayCorrection::LeftHand, leftHandPos, leftHandRot);
            ReplayCorrection::Apply(ReplayCorrection::RightHand, rightHandPos, rightHandRot);
        }
        else
        {
            ReplayCorrection::GetTrackedPose(GlobalNamespace::OVRPlugin::Node::HandLeft, leftHandPos, leftHandRot);
            ReplayCorrection::GetTrackedPose(GlobalNamespace::OVRPlugin::Node::HandRight, rightHandPos, rightHandRot);
            ReplayCorrection::GetTrackedPose(GlobalNamespace::OVRPlugin::Node::Head, headPos, headRot);

            if(!SceneEventManager::inGame)
                ReplayCorrection::SaveIfDirty();
        }

        VMC::VMCClient::SendHeadsetPos(headPos, headRot);
        VMC::VMCClient::SendControllerPos(leftHandPos, leftHandRot, 0);
        VMC::VMCClient::SendControllerPos(rightHandPos, rightHandRot, 1);

        VMC::VMCServer::Receive();

        auto trackers = VMC::VMCServer::availableTrackers;

        for(auto tracker : trackers)
        {
            auto name = tracker.name;
            UnityEngine::Transform* transform = nullptr;
            if(name == "human://WAIST") transform = waistTracker->transform;
            if(name == "human://CHEST") transform = chestTracker->transform;
            if(name == "human://LEFT_FOOT") transform = leftFootTracker->transform;
            if(name == "human://RIGHT_FOOT") transform = rightFootTracker->transform;
            if(name == "human://LEFT_KNEE") transform = leftKneeTracker->transform;
            if(name == "human://RIGHT_KNEE") transform = rightKneeTracker->transform;
            if(transform != nullptr)
            {
                Sombrero::FastVector3 lastPos = transform->position;
                transform->set_position(Sombrero::FastVector3::Lerp(lastPos, tracker.pos, UnityEngine::Time::get_deltaTime() * 20.0f));
                transform->set_rotation(tracker.rot);
            }
        }

        if(SceneEventManager::inGame && noodleTrack)
        {
            auto ikConfig = Config::ConfigManager::GetIKSettings();

            auto pos = noodleTrack->transform->position;
            auto offsetPos = UnityEngine::Vector3::op_Addition(pos, UnityEngine::Vector3(0.0f, ikConfig.groundOffset, 0.0f));

            if(UnityEngine::Vector3::Distance(transform->position, offsetPos) > 0.05f)
            {
                transform->position = offsetPos;
            }
            leftHandPos += pos;
            rightHandPos += pos;
            headPos += pos;

        }
        else
        {
            noodleTrack = nullptr;
            GetComponent<GroundOffsetObject*>()->set_enabled(true);
        }

        leftHandTarget->transform->position = leftHandPos;
        leftHandTarget->transform->rotation = leftHandRot;

        leftHandTarget->transform->Rotate({offset.rotX, offset.rotY, offset.rotZ});
        leftHandTarget->transform->Translate({offset.posX, offset.posY, offset.posZ});

        rightHandTarget->transform->position = rightHandPos;
        rightHandTarget->transform->rotation = rightHandRot;

        rightHandTarget->transform->Rotate({offset.rotX, offset.rotY, -offset.rotZ});
        rightHandTarget->transform->Translate({-offset.posX, offset.posY, offset.posZ});

        // the head target drives the head bone, so move it from the eyes back to the head bone
        headTarget->transform->position = headPos - headRot * headToEyesOffset;
        headTarget->transform->rotation = headRot;

        // the solver eases the body towards the targets over several frames, after the source switched that would show
        // the pose of the old one, e.g. the first frames of a render posed like the player was in the menu
        if(pendingPoseReset && (!usingReplayHead || replaySabersUpdated))
        {
            pendingPoseReset = false;
            ResetPose();
        }

#ifdef VRMQ_DIAG
        LogDiagnostics(this, headPos, inReplay);
#endif
    }

    void TargetManager::ResetPose()
    {
        const auto solver = vrik->solver;
        if(!solver->initiated || solver->LOD >= 2)
            return;

        // stand under the head facing where it faces, like the solver does when it comes back from culling
        // (noodle player tracks place the root themselves)
        const auto root = solver->root;
        const auto head = solver->spine->headTarget;
        if(head != nullptr && noodleTrack == nullptr)
        {
            const Sombrero::FastVector3 headPos = head->position;
            const Sombrero::FastVector3 rootPos = root->position;
            root->position = {headPos.x, rootPos.y, headPos.z};
            Sombrero::FastVector3 forward = Sombrero::FastQuaternion(head->rotation) * solver->spine->anchorRelativeToHead * Sombrero::FastVector3::forward();
            forward.y = 0.0f;
            if(forward.sqrMagnitude() > 0.0f)
                root->rotation = Sombrero::FastQuaternion::LookRotation(forward, Sombrero::FastVector3(root->up));
        }

        // start from the default pose, the feet are planted where it stands
        solver->FixTransforms();
        solver->Reset();
    }
}