#pragma once

#include "AtEntityStore.h"
#include "AtRp.h"
#include "AtRwLock.h"


namespace At
{

	// A wrapper around a Rp with a reader/writer spin lock that allows it to be read efficiently and frequently by many threads,
	// but also safely reset by any thread, and re-initialized on demand by a reader thread

	template <class T>
	class RpAnchor
	{
	public:
		Rp<T> Get() noexcept
		{
			ReadLocker readLocker { m_rwLock };
			return m_sp;
		}


		void Set(Rp<T> const& sp) noexcept
		{
			WriteLocker writeLocker { m_rwLock };
			m_sp = sp;
		}


		Rp<T> GetOrInit(std::function<void(Rp<T>& sp)> init)
		{
			Rp<T> sp;

			ReadLocker readLocker { m_rwLock };

			if (m_sp.Any())
			{
				// Pointer is available. Acquire our reference and release read lock
				sp.Set(m_sp);
			}
			else
			{
				readLocker.Release();
				WriteLocker writeLocker { m_rwLock };

				if (!m_sp.Any())
					init(m_sp);

				sp.Set(m_sp);
			}

			return sp;
		}


		Rp<T> GetOrLoad(EntityStore& store, Rp<StopCtl> const& stopCtl, std::function<void(Rp<T>& sp)> load)
		{
			return GetOrInit([&store, &stopCtl, &load, this] (Rp<T>& sp)
				{
					auto loadFromStore = [&sp, &load] { load(sp); };

					if (store.HaveTx())
						loadFromStore();
					else
						store.RunTx(stopCtl, typeid(*this), loadFromStore);

					EnsureThrow(sp.Any());
				} );
		}


		void Clear() noexcept
		{
			WriteLocker writeLocker { m_rwLock };
			NoExcept(m_sp.Clear());
		}


	private:
		RwLock m_rwLock;
		Rp<T>  m_sp;
	};

}
