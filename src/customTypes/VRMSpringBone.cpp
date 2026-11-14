#include "customTypes/VRMSpringBone.hpp"

#include <sombrero/shared/FastQuaternion.hpp>

DEFINE_TYPE(VRMQavatars, VRMSpringBone);

namespace VRMQavatars
{
	void VRMSpringBone::Awake()
	{
		Setup(false);
	}

	ArrayW<UnityEngine::Transform*> VRMSpringBone::GetChildren(UnityEngine::Transform* parent)
	{
		auto result = ArrayW<UnityEngine::Transform*>(parent->get_childCount());
		int num;
		for (int i = 0; i < parent->get_childCount(); i = num)
		{
			result[i] = parent->GetChild(i);
			num = i + 1;
		}
		return result;
	}

	void VRMSpringBone::FixedUpdate()
	{
		UpdateProcess(UnityEngine::Time::get_fixedDeltaTime());
	}

	void VRMSpringBone::Reset()
	{
		verlet.clear();
	}

	void VRMSpringBone::SetLocalRotationsIdentity()
	{
		for(const auto& logic : verlet)
		{
			logic->trans->set_localRotation(Sombrero::FastQuaternion::get_identity());
		}
	}

	void VRMSpringBone::Setup(const bool force)
	{
		if(rootBones)
		{
			if (force || this->initialLocalRotationMap.size() == 0)
			{
				initialLocalRotationMap = std::map<UnityEngine::Transform*, Sombrero::FastQuaternion>();
			}
			else
			{
				for (const auto& [fst, snd] : initialLocalRotationMap)
				{
					fst->set_localRotation(snd);
				}
				initialLocalRotationMap.clear();
			}
			verlet.clear();
			for (const auto& boneTrans : rootBones)
			{
				if(boneTrans != nullptr)
				{
					for (auto& child : boneTrans->GetComponentsInChildren<UnityEngine::Transform*>(true))
					{
						initialLocalRotationMap[child] = child->get_localRotation();
					}
					SetupRecursive(center, boneTrans);
				}
			}
		}
	}

	void VRMSpringBone::SetupRecursive(UnityEngine::Transform* center, UnityEngine::Transform* parent)
	{
		Sombrero::FastVector3 vector;
		Sombrero::FastVector3 vector2;
		if (parent->get_childCount() == 0)
		{
			const Sombrero::FastVector3 vector3 = UnityEngine::Vector3::op_Subtraction(parent->get_position(), parent->get_parent()->get_position());
			const float lossyScale = std::max(std::max(parent->get_lossyScale().x, parent->get_lossyScale().y), parent->get_lossyScale().z);
			const Sombrero::FastVector3 vector4 = UnityEngine::Vector3::op_Multiply(UnityEngine::Vector3::op_Addition(parent->get_position(), vector3.get_normalized()), 0.07f * lossyScale);
			vector = parent->get_worldToLocalMatrix().MultiplyPoint(vector4);
			vector2 = parent->get_lossyScale();
		}
		else
		{
			UnityEngine::Transform* transform = GetChildren(parent)->First();

			vector = transform->get_localPosition();
			vector2 = transform->get_lossyScale();
		}
		verlet.push_back(new VRMSpringBoneLogic(center, parent, Sombrero::FastVector3(vector.x * vector2.x, vector.y * vector2.y, vector.z * vector2.z)));
		for (size_t i = 0; i < parent->get_childCount(); i++)
		{
			const auto child = parent->GetChild(i);
			SetupRecursive(center, child);
		}
	}

	void VRMSpringBone::UpdateProcess(const float deltaTime)
	{
		if (verlet.size() == 0)
		{
			if (rootBones.size() == 0)
			{
				return;
			}
			Setup(false);
		}
		std::vector<SphereColliderLogic> colliders;
		for (const auto vrmspringBoneColliderGroup : colliderGroups)
		{
			if (vrmspringBoneColliderGroup != nullptr)
			{
				for (const auto sphereCollider : vrmspringBoneColliderGroup->colliders)
				{
					colliders.push_back(SphereColliderLogic(vrmspringBoneColliderGroup->get_transform(), sphereCollider));
				}
			}
		}
		const float num = stiffnessForce * deltaTime;
		const Sombrero::FastVector3 vector = gravityDir * (gravityPower * deltaTime);
		for (const auto& logic : verlet)
		{
			logic->SetRadius(hitRadius);
#ifdef VRMQ_DIAG
			// a tail inside a collider gets pushed out by this update
			for (const auto& collider : colliders)
			{
				const float r = logic->radius + collider.Radius;
				if ((logic->GetTail() - collider.Position).sqrMagnitude() <= r * r)
					diagHits++;
			}
#endif
			logic->Update(center, num, dragForce, vector, colliders);
		}
#ifdef VRMQ_DIAG
		LogDiag(colliders);
#endif
	}

#ifdef VRMQ_DIAG
	// once a second: the colliders this spring sees, and how far each tail is from the closest one
	void VRMSpringBone::LogDiag(const std::vector<SphereColliderLogic>& colliders)
	{
		const float now = UnityEngine::Time::get_time();
		if (now - diagLastLog < 1.0f)
			return;
		diagLastLog = now;
		std::vector<std::string> owners;
		for (const auto group : colliderGroups)
			if (group != nullptr)
				for (size_t i = 0; i < group->colliders.size(); i++)
					owners.push_back(group->get_gameObject()->get_name());
		auto fmtVec = [](Sombrero::FastVector3 v) { return fmt::format("({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z); };
		VRMLogger.info("[diag-spring] {} groups={} colliders={} hitRadius={:.3f} scale={:.3f} hits={}",
			comment ? static_cast<std::string>(comment) : "", colliderGroups.size(), colliders.size(), hitRadius,
			verlet.empty() ? 0.0f : verlet[0]->trans->get_lossyScale().y, diagHits);
		diagHits = 0;
		for (const auto& logic : verlet)
		{
			// the tip nodes carry no vertices
			if (logic->trans->get_childCount() == 0)
				continue;
			const Sombrero::FastVector3 tail = logic->GetTail();
			float best = 1e9f;
			size_t bestIndex = 0;
			for (size_t i = 0; i < colliders.size(); i++)
			{
				const float clearance = (tail - colliders[i].Position).get_magnitude() - logic->radius - colliders[i].Radius;
				if (clearance < best)
				{
					best = clearance;
					bestIndex = i;
				}
			}
			VRMLogger.info("[diag-spring]   {} tail={} closest={} at {} r={:.3f} clearance={:.3f}",
				static_cast<std::string>(logic->trans->get_name()), fmtVec(tail),
				bestIndex < owners.size() ? owners[bestIndex] : "-",
				colliders.empty() ? "-" : fmtVec(colliders[bestIndex].Position),
				colliders.empty() ? 0.0f : colliders[bestIndex].Radius, colliders.empty() ? 0.0f : best);
		}
	}
#endif
}