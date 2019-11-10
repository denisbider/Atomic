#include "AtIncludes.h"
#include "AtEntityKinds.h"

#include "AtEntityStore.h"

// These global objects MUST be initialized first.
// They are accessed by initializers of other global objects defined by entity macros.

#pragma warning (push)
#pragma warning (disable: 4073)
#pragma init_seg (lib)
#pragma warning (pop)

namespace At
{
	struct EntityKindEntry
	{
		EntityKindEntry(Entity const& sample, EntityCreator creator) : m_sample(sample), m_creator(creator) {}

		Entity const& m_sample;
		EntityCreator m_creator;
	};

	std::map<uint32, EntityKindEntry> g_entityKinds;
	bool g_entityKindsAccessed = false;
	

	uint32 VerifyEntityFields(EntityFieldInfo const* efi, EntityFieldInfo const*& key)
	{
		uint32 nrFields = 0;
		sizet nrKeys = 0;
		for (; !efi->IsPastEnd(); ++efi, ++nrFields)
		{
			if (KeyCat::IsKey(efi->m_keyCat))
			{
				EnsureThrow(FieldType::IsIndexable(efi->m_fieldType));

				if (KeyCat::IsStrKey(efi->m_keyCat))
					EnsureThrow(efi->m_fieldType == FieldType::Str);
				else
					EnsureThrow(efi->m_fieldType != FieldType::Str);

				key = efi;
				++nrKeys;
			}
		}

		if (nrKeys == 0)
			key = efi;
		else
			EnsureThrow(nrKeys == 1);

		return nrFields;
	}


	uint32 AddEntityKind(Entity const& sample, EntityCreator creator)
	{
		uint32 kind = sample.GetKind();
		EnsureThrow(kind != EntityKind::Unknown);
		EnsureThrow(!g_entityKindsAccessed);
		EnsureThrow(g_entityKinds.find(kind) == g_entityKinds.end());
		g_entityKinds.insert(std::make_pair(kind, EntityKindEntry(sample, creator)));
		return 1;
	}


	Entity const* TryGetEntitySample(uint32 kind)
	{
		g_entityKindsAccessed = true;

		std::map<uint32, EntityKindEntry>::const_iterator it = g_entityKinds.find(kind);
		if (it == g_entityKinds.end())
			return nullptr;

		return &(it->second.m_sample);
	}


	Entity const& GetEntitySample(uint32 kind)
	{
		Entity const* sample = TryGetEntitySample(kind);
		EnsureThrow(sample != nullptr);
		return *sample;
	}


	EntityCreator TryGetEntityCreator(uint32 kind)
	{
		g_entityKindsAccessed = true;		
		
		std::map<uint32, EntityKindEntry>::const_iterator it = g_entityKinds.find(kind);
		if (it == g_entityKinds.end())
			return nullptr;

		return it->second.m_creator;
	}


	EntityCreator GetEntityCreator(uint32 kind)
	{
		EntityCreator creator = TryGetEntityCreator(kind);
		EnsureThrow(creator != nullptr);
		return creator;
	}
}
