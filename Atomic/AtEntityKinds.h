#pragma once

#include "AtObjectStore.h"

namespace At
{
	struct EntityFieldInfo;
	class Entity;
	class EntityStore;

	typedef Rp<Entity> (*EntityCreator)(EntityStore* entityStore, ObjId parentId);

	struct EntityKind { enum E { Unknown = 0 }; };


	uint32 VerifyEntityFields(EntityFieldInfo const* efi, EntityFieldInfo const*& key);
	uint32 AddEntityKind(Entity const& sample, EntityCreator creator);

	Entity const* TryGetEntitySample(uint32 kind);
	Entity const& GetEntitySample(uint32 kind);

	EntityCreator TryGetEntityCreator(uint32 kind);		// Returns nullptr if entity kind not found
	EntityCreator GetEntityCreator(uint32 kind);		// Asserts if entity kind not found
}
