#pragma once

#include "AtEntity.h"
#include "AtTreeStore.h"


namespace At
{

	ENTITY_DECL_BEGIN(NullEntity)
	ENTITY_DECL_CLOSE()


	struct EntityAndRefObjId
	{
		// The entity ID of the child entity
		ObjId m_entityId;

		// The object ID of the ObjectStore object from which the entity ID was obtained.
		// Important for transaction consistency. If you load the child entity by its entity ID,
		// this should be passed as the reference object ID to EntityStore::GetEntity.
		ObjId m_refObjId;
	};


	struct EntityChildInfo : EntityAndRefObjId
	{
	public:
		// The kind ID of the child entity
		uint32 m_kind { EntityKind::Unknown };

		// The field type of the key field of the child entity
		uint32 m_keyFieldType { FieldType::PastEnd };

		union
		{
			// The value of the key field of the child entity if m_keyFieldType indicates it's an unsigned integer
			uint64 u_keyUInt;

			// The value of the key field of the child entity if m_keyFieldType indicates it's an signed integer
			int64  u_keySInt;

			// The value of the key field of the child entity if m_keyFieldType indicates it's a floating point value
			double u_keyFloat;
		};	

		// The value of the key field of the child entity if m_keyFieldType indicates it's a Time value
		Time m_keyTime;

		// The value of the key field of the child entity if m_keyFieldType indicates it's an ObjId value
		ObjId m_keyObjId;

		// The value of the key field of the child entity if m_keyFieldType indicates it's an Str value
		Str m_keyStr;

	public:
		EntityChildInfo() {}
		EntityChildInfo(Seq encodedKey, ObjId entityId, ObjId bucketId, Entity const*& kindSample);

		Str JsonEncodeKey() const;
	};



	class EntityStore : public Storage
	{
	public:
		EntityStore() : Storage(&m_treeStore) {}

		// - If expectedKind != EntityKind::Unknown, returns null if node not found; throws if node is empty, or has an entity of incorrect kind.
		// - If expectedKind == EntityKind::Unknown, returns null if node not found; returns NullEntity if node exists, but is empty.
		// - The refObjId parameter is important for transaction consistency. To the best extent possible, the application that uses EntityStore
		//   should keep track of which objects were used to load information such as IDs that are later used to load other objects. When the
		//   next object is loaded, the ID of the previous object that contained the necessary information should be passed as refObjId. This way,
		//   the database can detect if the application is trying to work with stale state (e.g. the reference object was removed or changed by
		//   another transaction), and can throw a RetryTxException. The application can also pass ObjId::None for the refObjId, but in this case
		//   the integrity of the reference cannot be checked, and the application must know in another way that it's not loading a stale ObjId.
		// - The object IDs used as references are more granular than entity IDs. An entity is stored as a TreeStore node, which in turn may be
		//   stored in multiple ObjectStore objects. Each ObjectStore object has a separate ObjId. For the reference check to work, the
		//   application must pass the ObjId of the specific ObjectStore object from which information about which object to load was obtained.
		Rp<Entity> GetEntity(ObjId entityId, ObjId refObjId, uint32 expectedKind);

		ChildCount RemoveEntityChildren (ObjId entityId) { return m_treeStore.RemoveNodeChildren (entityId); }
		void       RemoveEntity         (ObjId entityId) {        m_treeStore.RemoveNode         (entityId); }

		// Warning: this method is error prone because it requires choosing the correct refObjId.
		// Retrieving an entity using the incorrect refObjId may cause subtle inconsistencies and errors.
		// Instead of calling this directly, consider using Entity::GetReferencedEntity or Entity::GetParent.
		template <class EntityType>
		Rp<EntityType> GetEntityOfKind(ObjId entityId, ObjId refObjId)
			{ return GetEntity(entityId, refObjId, EntityType::Kind).DynamicCast<EntityType>(); }

		template <class EntityType>
		Rp<EntityType> GetEntityOfKind(EntityAndRefObjId const& ids)
			{ return GetEntityOfKind<EntityType>(ids.m_entityId, ids.m_refObjId); }

		template <EnumDir Direction = EnumDir::Forward>
		void EnumAllChildren(ObjId parentId, std::function<bool (EntityChildInfo const&)> onMatch);
		
		template <EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(ObjId parentId, uint32 kind, std::function<bool (EntityChildInfo const&)> onMatch);

		bool  AnyChildrenOfKind      (ObjId parentId, uint32 kind);
		ObjId FindChildIdWithSameKey (ObjId parentId, Entity const& e);
		bool  ChildWithSameKeyExists (ObjId parentId, Entity const& e) { return FindChildIdWithSameKey(parentId, e).Any(); }
	

		template <class ChildType>
		bool AnyChildrenOfKind(ObjId parentId)
			{ return AnyChildrenOfKind(parentId, ChildType::Kind); }


		template <class ChildType>
		Rp<ChildType> GetFirstChildOfKind(ObjId parentId)
		{
			Rp<ChildType> child;
			EnumAllChildrenOfKind<ChildType>(parentId, [&] (Rp<ChildType> const& c) { child = c; return false; } );
			return child;
		}


		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(ObjId parentId, std::function<bool (Rp<ChildType> const&)> onMatch)
		{
			EnumAllChildrenOfKind_EncodedKeysAndIds<ChildType, Direction>(parentId,
				[&] (Seq, ObjId childId, ObjId bucketId) -> bool
				{
					Rp<ChildType> child { new ChildType(*this, parentId) };
					child->m_entityId = childId;
					child->Load(bucketId);
					return onMatch(child);
				} );
		}

		
		template <class ChildType, class ContainerType, EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(ObjId parentId, ContainerType& container)
		{
			EnumAllChildrenOfKind<ChildType, Direction>(parentId, [&container] (Rp<ChildType> const& child) -> bool
				{ container.Add(child); return true; } );
		}


		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind_EncodedKeysAndIds(ObjId parentId, std::function<bool (Seq, ObjId, ObjId)> onMatch)
		{
			EnsureThrow(parentId != ObjId::None);
			EnsureThrow(ChildType::Kind + 1 > ChildType::Kind);
	
			Str keyFirst, keyBeyondLast;
			Entity::EncodeEmptyKey(keyFirst,      ChildType::Kind     );
			Entity::EncodeEmptyKey(keyBeyondLast, ChildType::Kind + 1 );

			m_treeStore.FindChildren<Direction>(parentId, keyFirst, keyBeyondLast, onMatch);
		}


		template <class ChildType>
		ChildCount RemoveAllChildrenOfKind(ObjId parentId)
		{
			EnsureThrow(parentId != ObjId::None);
			EnsureThrow(ChildType::Kind + 1 > ChildType::Kind);
	
			Str keyFirst, keyBeyondLast;
			Entity::EncodeEmptyKey(keyFirst,      ChildType::Kind     );
			Entity::EncodeEmptyKey(keyBeyondLast, ChildType::Kind + 1 );

			Vec<ObjId> children;		
			m_treeStore.FindChildren(parentId, keyFirst, keyBeyondLast,
				[&] (Seq, ObjId childId, ObjId) -> bool { children.Add(childId); return true; } );

			ChildCount childCount;
			childCount.m_nrDirectChildren = children.Len();
			childCount.m_nrAllDescendants = children.Len();

			for (ObjId const& childId : children)
			{
				ChildCount cc = m_treeStore.RemoveNodeChildren(childId);
				childCount.m_nrAllDescendants += cc.m_nrAllDescendants;

				m_treeStore.RemoveNode(childId);
			}

			return childCount;
		}


		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void FindChildren(ObjId parentId, typename ChildType::KeyType const& keyFirst, typename ChildType::KeyType const* keyBeyondLast,
			std::function<bool (Rp<ChildType> const&)> onMatch)
		{
			FindChildren_EncodedKeysAndIds<ChildType, Direction>(parentId, keyFirst, keyBeyondLast,
				[&] (Seq, ObjId childId, ObjId bucketId) -> bool
				{
					Rp<ChildType> child(new ChildType(*this, parentId));
					child->m_entityId = childId;
					child->Load(bucketId);				
					return onMatch(child);
				} );
		}


		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void FindChildren_EncodedKeysAndIds(ObjId parentId, typename ChildType::KeyType const& keyFirst, typename ChildType::KeyType const* keyBeyondLast,
			std::function<bool (Seq, ObjId, ObjId)> onMatch)
		{
			EnsureThrow(parentId != ObjId::None);
			EnsureThrow(FieldTypeOf(keyFirst) == ChildType::KeyField->m_fieldType);

			Str encodedKeyFirst, encodedKeyBeyondLast;
			Entity::EncodeKey   <ChildType>(encodedKeyFirst,      keyFirst);
			Entity::EncodeKeyOpt<ChildType>(encodedKeyBeyondLast, keyBeyondLast);

			m_treeStore.FindChildren<Direction>(parentId, encodedKeyFirst, encodedKeyBeyondLast, onMatch);
		}


		template <class ChildType>
		Rp<ChildType> FindChild(ObjId parentId, typename ChildType::KeyType const& key)
		{
			typename ChildType::KeyType        nextKey    { Entity::NextKey(key) };
			typename ChildType::KeyType const* nextKeyPtr { nullptr };
			if (key < nextKey)
				nextKeyPtr = &nextKey;

			Rp<ChildType> child;
			FindChildren<ChildType>(parentId, key, nextKeyPtr,
				[&] (Rp<ChildType> const& c) -> bool { child = c; return false; } );
			return child;
		}


		template <class ChildType>
		ObjId FindChildId(ObjId parentId, typename ChildType::KeyType const& key)
		{
			typename ChildType::KeyType        nextKey    { Entity::NextKey(key) };
			typename ChildType::KeyType const* nextKeyPtr { nullptr };
			if (key < nextKey)
				nextKeyPtr = &nextKey;

			ObjId childId;
			FindChildren_EncodedKeysAndIds<ChildType>(parentId, key, nextKeyPtr,
				[&] (Seq, ObjId id, ObjId) -> bool { childId = id; return false; } );
			return childId;
		}


		template <class ChildType>
		bool GetLastChildInfoOfKind(ObjId parentId, typename ChildType::KeyType& foundKey, ObjId& foundId, ObjId& foundBucketId)
		{
			bool found {};
			Str encodedKey;

			EnumAllChildrenOfKind_EncodedKeysAndIds<ChildType, EnumDir::Reverse>(parentId,
				[&] (Seq key, ObjId id, ObjId bucketId) -> bool
					{
						found = true;
						encodedKey.Set(key);
						foundId = id;
						foundBucketId = bucketId;
						return false;
					} );

			if (!found)
				return false;

			Seq reader { encodedKey };
			uint32 kind {};
			if (!DecodeUInt32(reader, kind) || kind != ChildType::Kind)
				return false;

			Entity::DecodeValE(reader, foundKey, Entity::FldEncDisp::AsKey);
			return true;
		}


		// A common usage pattern is to have entities with no fields to act as parents, so that things aren't
		// stored directly under ObjId::Root. This method helps create such entities, if they aren't created yet.
		template <class EntityType>
		Rp<EntityType> InitCategoryParent(ObjId parentId = ObjId::Root)
		{
			Rp<EntityType> e { GetFirstChildOfKind<EntityType>(parentId) };
			if (!e.Any())
			{
				e = new EntityType(*this, parentId);
				e->Insert_ParentExists();
			}
			return e;
		}


		template <class KeyType, class ChildType, class BatchState>
		void MultiTx_ProcessChildren(Exclusive excl, Rp<StopCtl> const& stopCtl, ObjId parentId, KeyType const& keyFirst, KeyType const* keyBeyondLast,
			std::function<void (BatchState&, Rp<ChildType> const&)> onMatch,
			std::function<void (BatchState&)> finalizeBatch,
			std::function<bool (BatchState&)> afterCommit)
		{
			EnsureThrow(parentId != ObjId::None);
			EnsureThrow(FieldTypeOf(keyFirst) == ChildType::KeyField->m_fieldType);

			Str encodedKeyFirst, encodedKeyBeyondLast;
			Entity::EncodeKey   <ChildType, KeyType>(encodedKeyFirst,      keyFirst);
			Entity::EncodeKeyOpt<ChildType, KeyType>(encodedKeyBeyondLast, keyBeyondLast);

			m_treeStore.MultiTx_ProcessChildren<BatchState>(excl, stopCtl, parentId, encodedKeyFirst, encodedKeyBeyondLast,
				[&] (BatchState& state, Seq, ObjId childId, ObjId bucketId)
				{
					Rp<ChildType> child(new ChildType(*this, parentId));
					child->m_entityId = childId;
					child->Load(bucketId);
					onMatch(state, child);
				},
				finalizeBatch,
				afterCommit);
		}


		template <class ChildType, class BatchState>
		void MultiTx_ProcessAllChildrenOfKind(Exclusive excl, Rp<StopCtl> const& stopCtl, ObjId parentId,
			std::function<void (BatchState&, Rp<ChildType> const&)> onMatch,
			std::function<void (BatchState&)> finalizeBatch,
			std::function<bool (BatchState&)> afterCommit)
		{
			EnsureThrow(parentId != ObjId::None);
			EnsureThrow(ChildType::Kind + 1 > ChildType::Kind);
	
			Str keyFirst, keyBeyondLast;
			Entity::EncodeEmptyKey(keyFirst,      ChildType::Kind     );
			Entity::EncodeEmptyKey(keyBeyondLast, ChildType::Kind + 1 );

			m_treeStore.MultiTx_ProcessChildren<BatchState>(excl, stopCtl, parentId, keyFirst, keyBeyondLast,
				[&] (BatchState& state, Seq, ObjId childId, ObjId bucketId)
				{
					Rp<ChildType> child(new ChildType(*this, parentId));
					child->m_entityId = childId;
					child->Load(bucketId);
					onMatch(state, child);
				},
				finalizeBatch,
				afterCommit);
		}


	private:
		TreeStore m_treeStore;

		friend class Entity;
	};



	// GetEntitiesCached
	// For retrieval of entities that may be frequently referenced by other entities.
	// Avoids having to repeatedly acquire ObjectStore lock and decoding.

	template <class T>
	class GetEntitiesCached
	{
	public:
		GetEntitiesCached(EntityStore& store) : m_store(store) {}

		Rp<T> GetEntity(ObjId entityId, ObjId refObjId)
		{
			EntityByIdMap<T>::It it { m_entities.Find(entityId) };
			if (it.Any())
				return *it;

			// Do not check for null; cache null results also
			Rp<T> entity { m_store.GetEntityOfKind<T>(entityId, refObjId) };
			m_entities.Add(entity);
			return entity;
		}

		Rp<T> GetParentOf(Entity const& x) { return GetEntity(x.m_parentId, x.m_entityId); }

	private:
		EntityStore& m_store;
		EntityByIdMap<T> m_entities;
	};

}
