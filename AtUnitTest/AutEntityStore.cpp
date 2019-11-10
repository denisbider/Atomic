#include "AutIncludes.h"
#include "AutMain.h"



namespace
{

	// Users
	// Stored under /

	ENTITY_DECL_BEGIN(Users)
	ENTITY_DECL_CLOSE()

	ENTITY_DEF_BEGIN(Users)
	ENTITY_DEF_CLOSE(Users)
	


	// User
	// Stored under /Users

	ENTITY_DECL_BEGIN(User)
	ENTITY_DECL_FLD_K(Str,    name, KeyCat::Key_Str_Unique_Insensitive)
	ENTITY_DECL_FIELD(Time,   lastSeenTime)
	ENTITY_DECL_FIELD(uint64, nrItemsQueued)
	ENTITY_DECL_FIELD(uint64, nrItemsProcessed)
	ENTITY_DECL_CLOSE()

	ENTITY_DEF_BEGIN(User)
	ENTITY_DEF_FIELD(User, name)
	ENTITY_DEF_FIELD(User, lastSeenTime)
	ENTITY_DEF_FIELD(User, nrItemsQueued)
	ENTITY_DEF_FIELD(User, nrItemsProcessed)
	ENTITY_DEF_CLOSE(User)



	// Queue
	// Stored under /

	ENTITY_DECL_BEGIN(Queue)
	ENTITY_DECL_CLOSE()

	ENTITY_DEF_BEGIN(Queue)
	ENTITY_DEF_CLOSE(Queue)



	// ItemState

	DESCENUM_DECL_BEGIN(ItemState)
	DESCENUM_DECL_VALUE(Unknown,    0)
	DESCENUM_DECL_VALUE(Scheduled,  100)
	DESCENUM_DECL_VALUE(InProgress, 200)
	DESCENUM_DECL_CLOSE()

	DESCENUM_DEF_BEGIN(ItemState)
	DESCENUM_DEF_VALUE(Unknown)
	DESCENUM_DEF_VALUE(Scheduled)
	DESCENUM_DEF_VALUE(InProgress)
	DESCENUM_DEF_CLOSE()



	// QueueItem
	// Stored under /Queue

	ENTITY_DECL_BEGIN(QueueItem)
	ENTITY_DECL_FLD_K(Time,      timeAdded, KeyCat::Key_NonStr_Multi)
	ENTITY_DECL_FLD_E(ItemState, state)
	ENTITY_DECL_FIELD(ObjId,     userId)
	ENTITY_DECL_CLOSE()

	ENTITY_DEF_BEGIN(QueueItem)
	ENTITY_DEF_FIELD(QueueItem, timeAdded)
	ENTITY_DEF_FLD_E(QueueItem, state)
	ENTITY_DEF_FIELD(QueueItem, userId)
	ENTITY_DEF_CLOSE(QueueItem)



	// Globals

	EntityStore g_store;
	Rp<Users> g_users;
	Rp<Queue> g_q1;
	Rp<Queue> g_q2;
	Rp<SharedEvent> g_q1NewItemEvent;
	Rp<SharedEvent> g_q2NewItemEvent;



	// UserThread

	class UserThread : public Thread
	{
	protected:
		void ThreadMain() override final;

		struct TxType_CreateUser {};
		struct TxType_RemoveUser {};
		struct TxType_QueueItem  {};
	};


	void UserThread::ThreadMain()
	{
		// Action weights
		uint32 const awRemoveUser = 1;
		uint32 const awQueueItem  = 10;
		uint32 const awTotal = awRemoveUser + awQueueItem;

		Str userName;
		uint nrActionsToLastSeenUpdate {};

		auto updateLastSeen = [&] (Rp<User> const& user)
			{
				user->f_lastSeenTime = Time::NonStrictNow();
				nrActionsToLastSeenUpdate = 5;
			};

		auto maybeUpdateLastSeen = [&] (Rp<User> const& user)
			{
				EnsureThrow(nrActionsToLastSeenUpdate > 0);
				if (!--nrActionsToLastSeenUpdate)
				{
					updateLastSeen(user);
					user->Update();
				}
			};
		
		while (true)
		{
			// Create user if not currently created
			if (!userName.Any())
			{
				userName = Token::Generate();

				g_store.RunTx(GetStopCtl(), typeid(TxType_CreateUser), [&]
					{
						Rp<User> user = new User(Entity::ChildOf, g_users.Ref());
						user->f_name = userName;
						updateLastSeen(user);
						user->Insert_ParentLoaded();
					} );
			}

			uint32 selector = Crypt::GenRandomNr32(awTotal - 1);

			auto selectorIsLessThanOrSubtract = [&] (uint32 aw) -> bool
				{
					if (selector < aw)
						return true;

					selector -= aw;
					return false;
				};

			if (selectorIsLessThanOrSubtract(awRemoveUser))
			{
				g_store.RunTx(GetStopCtl(), typeid(TxType_RemoveUser), [&]
					{
						Rp<User> user = g_users->FindChild<User>(userName);
						user->Remove();
					} );

				userName.Clear();
			}
			else if (selectorIsLessThanOrSubtract(awQueueItem))
			{
				g_store.RunTx(GetStopCtl(), typeid(TxType_QueueItem), [&]
					{
						Rp<User> user = g_users->FindChild<User>(userName);
						updateLastSeen(user);
						++(user->f_nrItemsQueued);
						user->Update();

						Rp<Queue> q;
						Rp<SharedEvent> newItemEvent;

						if (Crypt::GenRandomNr32(1) == 0)
						{
							q = g_q1;
							newItemEvent = g_q1NewItemEvent;
						}
						else
						{
							q = g_q2;
							newItemEvent = g_q2NewItemEvent;
						}

						Rp<QueueItem> qItem = new QueueItem(Entity::ChildOf, q.Ref());
						qItem->f_timeAdded = Time::NonStrictNow();
						qItem->f_state = ItemState::Scheduled;
						qItem->f_userId = user->m_entityId;
						qItem->Insert_ParentExists();

						g_store.AddPostCommitAction( [newItemEvent] { newItemEvent->Signal(); } );
					} );
			}
			else
				EnsureAbort(!"Unexpected selector value");
		}
	}



	// QueueProcessor

	class QueueProcessor : public Thread
	{
	public:
		void SetQueueParameters(Rp<Queue> const& q, Rp<SharedEvent> newItemEvent)
			{ m_q = q; m_newItemEvent = newItemEvent; }

	protected:
		Rp<Queue> m_q;
		Rp<SharedEvent> m_newItemEvent;

		void ThreadMain() override final;

		struct TxType_BeginProcessing {};
		struct TxType_CompleteProcessing {};
	};


	void QueueProcessor::ThreadMain()
	{
		while (true)
		{
			AbortableWait(m_newItemEvent->Handle());

			m_newItemEvent->ClearSignal();

			RpVec<QueueItem> items;

			g_store.RunTx(GetStopCtl(), typeid(TxType_BeginProcessing), [&]
				{
					items.Clear();

					m_q->EnumAllChildrenOfKind<QueueItem>( [&] (Rp<QueueItem> c) -> bool
						{
							if (c->f_state == ItemState::Scheduled)
								items.Add(c);
							return true;
						} );

					for (Rp<QueueItem> const& item : items)
					{
						item->f_state = ItemState::InProgress;
						item->Update();
					}
				} );

			Sleep(0);

			g_store.RunTx(GetStopCtl(), typeid(TxType_CompleteProcessing), [&]
				{
					for (Rp<QueueItem> const& item : items)
					{
						Rp<User> user = g_store.GetEntityOfKind<User>(item->f_userId, ObjId::None);
						if (user.Any())
						{
							++(user->f_nrItemsProcessed);
							user->Update();
						}

						item->Remove();
					}
				} );
		}
	}

} // anon



// EntityStoreTests

void EntityStoreTests(int argc, char** argv)
{
	bool removeStore { true };
	uint32 writeFailOdds {};

	for (int i=2; i<argc; ++i)
	{
		Seq arg = argv[i];
		if (arg.StripPrefixInsensitive("-writeFailOdds="))
			writeFailOdds = arg.ReadNrUInt32Dec();
		else if (arg.EqualInsensitive("-noRemove"))
			removeStore = false;
		else
		{
			Console::Out("Unrecognized parameter. Supported: -writeFailOdds=..., -noRemove\r\n");
			return;
		}
	}

	try
	{
		Crypt::Initializer cryptInit;

		Str storePath = GetModuleSubdir("EntityStoreTest");
		if (removeStore)
			RemoveDirAndSubdirsIfExists(storePath);

		g_store.SetDirectory(storePath);
		if (writeFailOdds)
			g_store.SetWritePlanTest(true, writeFailOdds);
		g_store.Init();

		g_store.RunTxExclusive( []
			{
				g_users = g_store.InitCategoryParent<Users>();
				g_q1    = g_store.InitCategoryParent<Queue>();
				g_q2    = g_store.InitCategoryParent<Queue>();
			} );

		g_q1NewItemEvent = new SharedEvent(Event::CreateManual);
		g_q2NewItemEvent = new SharedEvent(Event::CreateManual);

		StopCtl* pStopCtl = new StopCtl;		// Avoids bogus unreachable code warning in Release mode
		Rp<StopCtl> stopCtl { pStopCtl };

		ThreadPtr<WaitEsc> waitEsc { Thread::Create };
		waitEsc->Start(stopCtl);

		ThreadPtr<UserThread> userThread1 { Thread::Create };
		ThreadPtr<UserThread> userThread2 { Thread::Create };
		userThread1->SetStopCtl(stopCtl);
		userThread2->SetStopCtl(stopCtl);

		ThreadPtr<QueueProcessor> qpc1 { Thread::Create };
		ThreadPtr<QueueProcessor> qpc2 { Thread::Create };
		qpc1->SetStopCtl(stopCtl);
		qpc2->SetStopCtl(stopCtl);
		qpc1->SetQueueParameters(g_q1, g_q1NewItemEvent);
		qpc2->SetQueueParameters(g_q2, g_q2NewItemEvent);

		Console::Out("Starting test. Press Esc to stop and display stats\r\n");

		ULONGLONG startTicks = GetTickCount64();

		userThread1->Start();
		userThread2->Start();
		qpc1->Start();
		qpc2->Start();

		stopCtl->WaitAll();

		ULONGLONG endTicks = GetTickCount64();
		double elapsedSeconds = ((double) SatSub(endTicks, startTicks)) / 1000.0;

		Storage::Stats stats = g_store.GetStats(Storage::Stats::Keep);

		Str msg;
		msg.ReserveExact(1000);
		msg.Set("Elapsed seconds: ").Dbl(elapsedSeconds).Add("\r\n")
		   .Set("Stop reason: ").Add(stopCtl->StopReason()).Add("\r\n");

		auto addStat = [&msg, elapsedSeconds] (char const* prefix, sizet value)
			{
				sizet perSecond = (sizet) (((double) value) / elapsedSeconds);
				msg.Add(prefix).UInt(value, 10, 0, CharCase::Upper, 7)
				   .Add(" (").UInt(perSecond, 10, 0, CharCase::Upper, 5).Add(" per second)\r\n");
			};

		addStat("nrRunTxExclusive:       ", stats.m_nrRunTxExclusive       );
		addStat("nrTryRunTxNonExclusive: ", stats.m_nrTryRunTxNonExclusive );
		addStat("nrNonExclusiveGiveUps:  ", stats.m_nrNonExclusiveGiveUps  );
		addStat("nrStartTx:              ", stats.m_nrStartTx              );
		addStat("nrCommitTx:             ", stats.m_nrCommitTx             );
		addStat("nrAbortTx:              ", stats.m_nrAbortTx              );

		Console::Out(msg);
	}
	catch (Exception const& e)
	{
		Str msg = "EntityStoreTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
