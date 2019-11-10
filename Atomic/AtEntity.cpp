#include "AtIncludes.h"
#include "AtEntity.h"

#include "AtEntityStore.h"


namespace At
{

	// Fields

	char const* FieldType::GetName(uint ft)
	{
		switch (ft)
		{
		case PastEnd:		return "PastEnd";

		case Bool:			return "Bool";
		case Entity:		return "Entity";
		case UInt:			return "UInt";
		case SInt:			return "SInt";
		case Float:			return "Float";
		case Time:			return "Time";
		case ObjId:			return "ObjId";
		case Str:			return "Str";

		case OptDyn:		return "OptDyn";
		case OptEnt:		return "OptEnt";
		case OptBool:		return "OptBool";
		case OptUInt:		return "OptUInt";
		case OptSInt:		return "OptSInt";
		case OptFloat:		return "OptFloat";
		case OptTime:		return "OptTime";
		case OptObjId:		return "OptObjId";
		case OptStr:		return "OptStr";

		case VecDyn:		return "VecDyn";
		case VecEnt:		return "VecEnt";
		case VecBool:		return "VecBool";
		case VecUInt:		return "VecUInt";
		case VecSInt:		return "VecSInt";
		case VecFloat:		return "VecFloat";
		case VecTime:		return "VecTime";
		case VecObjId:		return "VecObjId";
		case VecStr:		return "VecStr";

		default:			return "<unknown field type>";
		}
	}



	// Entity

	Entity::Entity(EntityStore* store, uint32 kind, char const* kindName, uint32 nrFields, EntityFieldInfo const* fields, EntityFieldInfo const* key, ObjId parentId)
		: m_store(store)
		, m_kind(kind)
		, m_kindName(kindName)
		, m_nrFields(nrFields)
		, m_fields(fields)
		, m_key(key)
		, m_parentId(parentId)
		, m_hasChildrenState(HasChildrenState::Unknown)
	{
	}


	EntityFieldInfo const* Entity::GetFieldByName(Seq name) const
	{
		for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
			if (name.EqualExact(efi->m_fieldName))
				return efi;
		return 0;
	}


	void Entity::ReInitFields()
	{
		for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
			switch (efi->m_fieldType)
			{
			case FieldType::Entity:		FieldRef<Entity      >(*this, efi->m_offset).ReInitFields(); break;
			case FieldType::Bool:		FieldRef<bool        >(*this, efi->m_offset) = false;        break;
			case FieldType::Enum:		FieldRef<uint32      >(*this, efi->m_offset) = 0;            break;
			case FieldType::UInt:		FieldRef<uint64      >(*this, efi->m_offset) = 0;            break;
			case FieldType::SInt:		FieldRef<int64       >(*this, efi->m_offset) = 0;            break;
			case FieldType::Float:		FieldRef<double      >(*this, efi->m_offset) = 0.0;          break;
			case FieldType::Time:		FieldRef<uint64      >(*this, efi->m_offset) = 0;            break;
			case FieldType::ObjId:		FieldRef<ObjId       >(*this, efi->m_offset) = ObjId();      break;
			case FieldType::Str:		FieldRef<Str         >(*this, efi->m_offset).Free();         break;

			case FieldType::OptDyn:		FieldRef<Rp<Entity   >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptEnt:		FieldRef<EntOptBase   >(*this, efi->m_offset).Clear();      break;
			case FieldType::OptBool:	FieldRef<Opt<bool    >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptUInt:	FieldRef<Opt<uint64  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptSInt:	FieldRef<Opt<int64   >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptFloat:	FieldRef<Opt<double  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptTime:	FieldRef<Opt<uint64  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptObjId:	FieldRef<Opt<ObjId   >>(*this, efi->m_offset).Clear();      break;
			case FieldType::OptStr:		FieldRef<Opt<Str     >>(*this, efi->m_offset).Clear();      break;

			case FieldType::VecDyn:		FieldRef<RpVec<Entity>>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecEnt:		FieldRef<EntVecBase   >(*this, efi->m_offset).Clear();      break;
			case FieldType::VecBool:	FieldRef<Vec<bool    >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecUInt:	FieldRef<Vec<uint64  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecSInt:	FieldRef<Vec<int64   >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecFloat:	FieldRef<Vec<double  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecTime:	FieldRef<Vec<uint64  >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecObjId:	FieldRef<Vec<ObjId   >>(*this, efi->m_offset).Clear();      break;
			case FieldType::VecStr:		FieldRef<Vec<Str     >>(*this, efi->m_offset).Clear();      break;

			default:
				EnsureThrow(!"Unrecognized field type");
			}
	}


	void Entity::Load(ObjId refObjId)
	{
		EnsureThrow(m_store != nullptr);
		EnsureThrow(m_entityId != ObjId::None);

		TreeStore::Node node;
		node.m_nodeId = m_entityId;
		EnsureAbort(m_store->m_treeStore.GetNodeById(node, refObjId));
		m_contentObjId = node.m_contentObjId;

		m_parentId = node.m_parentId;
		m_hasChildrenState = node.m_hasChildren ? HasChildrenState::Yes : HasChildrenState::No;
	
		Seq data { node.m_data };
	
		uint32 kind;
		EnsureAbort(DecodeUInt32(data, kind));
		EnsureAbort(kind == m_kind);
	
		DecodeFieldsE(data);
		EnsureAbort(data.n == 0);
	}


	void Entity::Put(PutType::E putType, ObjId parentRefObjId)
	{
		EnsureThrow(m_store != nullptr);
		EnsureThrow((putType == PutType::New) == (m_entityId == ObjId::None));

		TreeStore::Node node;
		node.m_parentId = m_parentId;

		Enc::Meter keyMeter = node.m_key.FixMeter(EncodeKey_Size());
		EncodeKey(node.m_key);
		EnsureThrow(keyMeter.Met());

		Enc::Meter dataMeter = node.m_data.FixMeter(4 + EncodeFields_Size());
		EncodeUInt32(node.m_data, m_kind);
		EncodeFields(node.m_data);
		EnsureThrow(dataMeter.Met());

		if (putType == PutType::New)
		{
			// Insert new entity
			EnsureThrow(m_parentId != ObjId::None);
			m_store->m_treeStore.InsertNode(node, parentRefObjId, HaveKey() && UniqueKey());
			m_entityId = node.m_nodeId;
		}
		else
		{
			// Replace existing entity
			if (m_parentId == ObjId::None)
				EnsureThrow(m_entityId == ObjId::Root);
			else
			{
				node.m_nodeId = m_entityId;
				m_store->m_treeStore.ReplaceNode(node, HaveKey() && UniqueKey());
			}
		}

		m_contentObjId = node.m_contentObjId;
	}


	ChildCount Entity::RemoveChildren()
	{
		EnsureThrow(m_store != nullptr);
		return m_store->RemoveEntityChildren(m_entityId);
	}


	void Entity::Remove()
	{
		EnsureThrow(m_store != nullptr);
		m_store->RemoveEntity(m_entityId);
	}

}
