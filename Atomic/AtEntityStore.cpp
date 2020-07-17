#include "AtIncludes.h"
#include "AtEntityStore.h"


namespace At
{

	// NullEntity

	ENTITY_DEF_BEGIN(NullEntity)
	ENTITY_DEF_CLOSE(NullEntity)



	// EntityChildInfo

	EntityChildInfo::EntityChildInfo(Seq encodedKey, ObjId entityId, ObjId bucketId, Entity const*& kindSample)
	{
		EnsureThrow(DecodeUInt32(encodedKey, m_kind));
			
		if (!kindSample || m_kind != kindSample->GetKind())
			kindSample = &GetEntitySample(m_kind);

		EntityFieldInfo const* key = kindSample->GetKey();
		m_keyFieldType = key->m_fieldType;

		switch (key->m_fieldType)
		{
		case FieldType::PastEnd:	break;
		case FieldType::UInt:		Entity::DecodeValE(encodedKey, u_keyUInt,  Entity::FldEncDisp::AsKey); break;
		case FieldType::SInt:		Entity::DecodeValE(encodedKey, u_keySInt,  Entity::FldEncDisp::AsKey); break;
		case FieldType::Float:		Entity::DecodeValE(encodedKey, u_keyFloat, Entity::FldEncDisp::AsKey); break;
		case FieldType::Time:		Entity::DecodeValE(encodedKey, m_keyTime,  Entity::FldEncDisp::AsKey); break;
		case FieldType::ObjId:		Entity::DecodeValE(encodedKey, m_keyObjId, Entity::FldEncDisp::AsKey); break;
		case FieldType::Str:		Entity::DecodeValE(encodedKey, m_keyStr,   Entity::FldEncDisp::AsKey); break;
		default:					EnsureThrow(!"Unrecognized key type");
		}
	
		m_entityId = entityId;
		m_refObjId = bucketId;
	}



	// EntityStore

	Rp<Entity> EntityStore::GetEntity(ObjId entityId, ObjId refObjId, uint32 expectedKind)
	{
		TreeStore::Node node;
		node.m_nodeId = entityId;
		if (!m_treeStore.GetNodeById(node, refObjId))
			return Rp<Entity>();
	
		Rp<Entity> entity;
		if (!node.m_data.Any())
		{
			EnsureThrow(expectedKind == EntityKind::Unknown);
			entity.Set(new NullEntity(*this, node.m_parentId));
		}
		else
		{
			Seq data { node.m_data };
	
			uint32 kind;
			EnsureThrow(DecodeUInt32(data, kind));
			if (expectedKind != EntityKind::Unknown)
				EnsureThrow(kind == expectedKind);
	
			entity.Set(GetEntityCreator(kind)(this, node.m_parentId));
			entity->DecodeFieldsE(data);
			EnsureThrow(data.n == 0);
		}

		entity->m_entityId = entityId;
		entity->m_contentObjId = node.m_contentObjId;
		entity->m_hasChildrenState = node.m_hasChildren ? Entity::HasChildrenState::Yes : Entity::HasChildrenState::No;
		return entity;
	}


	template <EnumDir Direction>
	void EntityStore::EnumAllChildren(ObjId parentId, std::function<bool (EntityChildInfo const&)> onMatch)
	{
		EnsureThrow(parentId != ObjId::None);
		Entity const* kindSample = nullptr;
		m_treeStore.EnumAllChildren(parentId,
			[&] (Seq encodedKey, ObjId objId, ObjId bucketId) -> bool
				{ return onMatch(EntityChildInfo(encodedKey, objId, bucketId, kindSample)); } );
	}

	template void EntityStore::EnumAllChildren<EnumDir::Forward>(ObjId parentId, std::function<bool (EntityChildInfo const&)> onMatch);
	template void EntityStore::EnumAllChildren<EnumDir::Reverse>(ObjId parentId, std::function<bool (EntityChildInfo const&)> onMatch);


	template <EnumDir Direction>
	void EntityStore::EnumAllChildrenOfKind(ObjId parentId, uint32 kind, std::function<bool (EntityChildInfo const&)> onMatch)
	{
		EnsureThrow(parentId != ObjId::None);
		EnsureThrow(kind + 1 > kind);
	
		Str keyFirst, keyBeyondLast;
		Entity::EncodeEmptyKey(keyFirst,      kind);
		Entity::EncodeEmptyKey(keyBeyondLast, kind + 1);

		Entity const* kindSample = nullptr;
		m_treeStore.FindChildren(parentId, keyFirst, keyBeyondLast,
			[&] (Seq encodedKey, ObjId objId, ObjId bucketId) -> bool
				{ return onMatch(EntityChildInfo(encodedKey, objId, bucketId, kindSample)); } );
	}

	template void EntityStore::EnumAllChildrenOfKind<EnumDir::Forward>(ObjId parentId, uint32 kind, std::function<bool (EntityChildInfo const&)> onMatch);
	template void EntityStore::EnumAllChildrenOfKind<EnumDir::Reverse>(ObjId parentId, uint32 kind, std::function<bool (EntityChildInfo const&)> onMatch);


	bool EntityStore::AnyChildrenOfKind(ObjId parentId, uint32 kind)
	{
		bool anyMatches = false;
		EnumAllChildrenOfKind(parentId, kind, [&] (EntityChildInfo const&) -> bool { anyMatches = true; return false; } );
		return anyMatches;		
	}


	ObjId EntityStore::FindChildIdWithSameKey(ObjId parentId, Entity const& e)
	{
		EnsureThrow(parentId != ObjId::None);

		Str key;
		e.EncodeKey(key);
		key.Byte(0);

		Seq keyFirst = Seq(key).RevDropByte();
		Seq keyBeyondLast = key;

		ObjId foundId;
		m_treeStore.FindChildren(parentId, keyFirst, keyBeyondLast,
			[&] (Seq, ObjId objId, ObjId) -> bool
				{ foundId = objId; return false; } );
		return foundId;
	}

}
