#include "AutIncludes.h"
#include "AutMain.h"



namespace
{

	// LargeEntity

	ENTITY_DECL_BEGIN(LargeEntity)
	ENTITY_DECL_FLD_K(uint64, seqNr, KeyCat::Key_NonStr_Unique)
	ENTITY_DECL_FIELD(uint64, counter)
	ENTITY_DECL_FIELD(Str,    content)
	ENTITY_DECL_CLOSE()

	ENTITY_DEF_BEGIN(LargeEntity)
	ENTITY_DEF_FIELD(LargeEntity, seqNr)
	ENTITY_DEF_FIELD(LargeEntity, counter)
	ENTITY_DEF_FIELD(LargeEntity, content)
	ENTITY_DEF_CLOSE(LargeEntity)


	// Globals

	EntityStore g_store;

} // anon


void LargeEntitiesTests()
{
	try
	{
		Crypt::Initializer cryptInit;

		Str storePath = GetModuleSubdir("LargeEntitiesTest");
		g_store.SetDirectory(storePath);
		g_store.Init();

		Console::Out(Str("EntityStore initialized, ObjectStore::NrDataFiles: ").UInt(ObjectStore::NrDataFiles).Add("\r\n"));

		RpVec<LargeEntity> entities;
		g_store.RunTxExclusive( [&]
			{ g_store.EnumAllChildrenOfKind<LargeEntity>(ObjId::Root, entities); });

		if (!entities.Any())
		{
			Console::Out("No large entities found, creating");
			Time startTime = Time::NonStrictNow();
			sizet lastSizeUsed;
			sizet nrEntitiesInserted {};

			g_store.RunTxExclusive( [&]
				{
					double sizeDbl = 1000.0;
					double const sizeMul = 1.0075;
					for (sizet i=0; i!=1000; ++i)
					{
						lastSizeUsed = (sizet) sizeDbl;
						Rp<LargeEntity> e = new LargeEntity(g_store, ObjId::Root);
						e->f_seqNr = i;
						e->f_content.ResizeExact(lastSizeUsed, 'a');
						e->Insert_ParentExists();
						++nrEntitiesInserted;
						sizeDbl *= sizeMul;
						if ((i%100) == 99)
							Console::Out(".");
					}
				} );

			Time endTime = Time::NonStrictNow();
			Time elapsed = endTime - startTime;
			Console::Out(Str("\r\nInserted ").UInt(nrEntitiesInserted).Add(" entities in ").Obj(elapsed, TimeFmt::DurationMilliseconds)
				.Add(", last size used ").UInt(lastSizeUsed).Add("\r\n"));
		}
		else
		{
			Console::Out(Str().UInt(entities.Len()).Add(" large entities found, incrementing counters"));
			Time startTime = Time::NonStrictNow();
			sizet nrEntitiesUpdated {};

			g_store.RunTxExclusive( [&]
				{
					for (Rp<LargeEntity> const& e : entities)
					{
						++(e->f_counter);
						e->Update();
						if (((nrEntitiesUpdated++)%100) == 99)
							Console::Out(".");
					}
				} );

			Time endTime = Time::NonStrictNow();
			Time elapsed = endTime - startTime;
			Console::Out(Str("\r\n").UInt(nrEntitiesUpdated).Add(" entities updated in ").Obj(elapsed, TimeFmt::DurationMilliseconds).Add("\r\n"));
		}
	}
	catch (Exception const& e)
	{
		Str msg = "LargeEntitiesTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
