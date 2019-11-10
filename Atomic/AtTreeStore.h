#pragma once

#include "AtObjectStore.h"


namespace At
{

	struct ChildCount
	{
		sizet m_nrDirectChildren {};
		sizet m_nrAllDescendants {};
	};

	class TreeStore : public Storage
	{
	public:
		// Exposed ObjectStore methods and types
		TreeStore() : Storage(&m_objectStore) {}

		void Init();

	public:
		struct Node
		{
			ObjId m_parentId;
			ObjId m_nodeId;
			ObjId m_contentObjId;
			Str   m_key;
			Str   m_data;
			bool  m_hasChildren {};

			sizet EncodedSize () const { return ObjId::EncodedSize + EncodeVarStr_Size(m_key.Len()) + EncodeVarStr_Size(m_data.Len()); }
			void  Encode      (Enc& enc) const;
			bool  Decode      (Seq& s);

			static bool SkipDecode(Seq& s);
		};

		static Str const KeyBeyondLast;
		static Str NextKey(Seq key) { return Str(key).Byte(0); }

		// Fields in:  m_parentId, m_key, m_data
		// Fields out: m_nodeId, m_contentObjId
		void InsertNode(Node& node, ObjId parentRefObjId, bool uniqueKey);

		// Fields in:  m_nodeId
		// Fields out: m_parentId, m_contentObjId, m_key, m_data, m_hasChildren
		bool GetNodeById(Node& node, ObjId refObjId);

		void FindChildren_Forward(ObjId parentId, Seq keyFirst, Seq keyBeyondLast, std::function<bool (Seq, ObjId, ObjId)> onMatch);
		void FindChildren_Reverse(ObjId parentId, Seq keyFirst, Seq keyBeyondLast, std::function<bool (Seq, ObjId, ObjId)> onMatch);
		
		template <EnumDir Direction = EnumDir::Forward>
		void FindChildren(ObjId parentId, Seq keyFirst, Seq keyBeyondLast, std::function<bool (Seq, ObjId, ObjId)> onMatch)
		{
			if (Direction == EnumDir::Forward)
				FindChildren_Forward(parentId, keyFirst, keyBeyondLast, onMatch);
			else
				FindChildren_Reverse(parentId, keyFirst, keyBeyondLast, onMatch);
		}

		// Fields in:  m_parentId, m_nodeId, m_key, m_data
		// Fields out: m_contentObjId
		void ReplaceNode(Node& node, bool uniqueKey);

		// Returns the number of children nodes removed
		ChildCount RemoveNodeChildren(ObjId nodeId);
		void RemoveNode(ObjId nodeId);

		void EnumAllChildren(ObjId parentId, std::function<bool (Seq, ObjId, ObjId)> onMatch)
			{ FindChildren(parentId, Seq(), KeyBeyondLast, onMatch); }

		template <class BatchState>											// BatchState is reconstructed at start of each batch transaction or Tx attempt
		void MultiTx_ProcessChildren(Exclusive excl, Rp<StopCtl> const& stopCtl, ObjId parentId, Seq keyFirst, Seq keyBeyondLast,
			std::function<void (BatchState&, Seq, ObjId, ObjId)> onMatch,	// Called within Tx. Should track any state in BatchState or datastore, not modify overall operation state
			std::function<void (BatchState&)> finalizeBatch,				// Called within Tx. Should track any state in BatchState or datastore, not modify overall operation state
			std::function<bool (BatchState&)> afterCommit)					// Called after each Tx. Should commit any BatchState info to overall operation state, then clear BatchState
		{
			Str nextKey = keyFirst;
			bool haveMore;

			do
			{
				BatchState state;				
				auto batchTx = [&]
					{
						haveMore = false;
						Reconstruct(state);

						ObjId batchBucketId;
						Str lastKey;

						FindChildren(parentId, nextKey, keyBeyondLast,
							[&] (Seq key, ObjId childId, ObjId bucketId)
							{
								if (!batchBucketId.Any())
									batchBucketId = bucketId;
								else if (bucketId != batchBucketId && key > lastKey)
								{
									haveMore = true;
									nextKey = key;
									return false;
								}

								lastKey = key;
								onMatch(state, key, childId, bucketId);
								return true;
							} );

						finalizeBatch(state);
					};

				if (excl == Exclusive::No)
					RunTx(stopCtl, typeid(BatchState), batchTx);
				else
					RunTxExclusive(batchTx);

				if (!afterCommit(state))
					break;
			}
			while (haveMore);
		}

		template <class BatchState>
		void MultiTx_ProcessAllChildren(Exclusive excl, Rp<StopCtl> const& stopCtl, ObjId parentId,
			std::function<void (BatchState&, Seq, ObjId, ObjId)> onMatch,
			std::function<void (BatchState&)> finalizeBatch,
			std::function<bool (BatchState&)> afterCommit)
		{
			MultiTx_ProcessChildren<BatchState>(excl, stopCtl, parentId, Seq(), KeyBeyondLast, onMatch, finalizeBatch, afterCommit);
		}

	private:
		enum
		{
			MaxRootBucketSize          = ObjectStore::MaxSingleReadObjectSize - ObjId::EncodedSize - 1,		// External node with max size root bucket fits into one ObjectStore read
			ExternalNodeSizeThreshold  = ObjectStore::MaxDoubleReadObjectSize - MaxRootBucketSize,			// Internal node with max size root bucket fits into two ObjectStore reads
			MaxNonRootBucketSize       = ObjectStore::MaxSingleReadObjectSize,								// Non-root buckets do not need provisions for info beyond their entries
			BucketJoinThreshold        = MaxNonRootBucketSize / 4,											// Only non-root buckets are joined. Threshold is about half after-split size
			MaxKeySize                 = BucketJoinThreshold - 1 - (1 + 2 + ObjId::EncodedSize),			// Each bucket entry must be smaller than BucketJoinThreshold
			MaxPathSteps               = 48,																// Limits maximum number of children to no less than 2^MaxPathSteps
		};

		struct NodeType { enum E { Internal = 0, External = 1 }; };

		struct BucketEntry
		{
			Seq m_key;
			ObjId m_objId;

			sizet EncodedSize() const { return EncodeVarStr_Size(m_key.n) + ObjId::EncodedSize; }
			void Encode(Enc& enc) const;
			bool Decode(Seq& s);
		};

		struct FindKeyResult { enum E { Found, Empty, FirstKeyEqual, FirstKeyGreater }; };

		class ChildBucket
		{
		public:
			enum { MinimumEncodedSize = 1 + 4 };

			ChildBucket(ObjId objId) : m_objId(objId) {}

			ObjId              GetObjId               ()             const { return m_objId; }
			void               SetObjId               (ObjId objId)        { m_objId = objId; }

			bool               Dirty                  ()             const { return m_dirty; }

			uint               Height                 ()             const { return m_height; }
			BucketEntry const& operator[]             (sizet i)      const { return m_entries[i]; }
			bool               Any                    ()             const { return m_entries.Any(); }
			sizet              NrEntries              ()             const { return m_entries.Len(); }

			void               ReplaceEntryKey        (sizet index, Seq newKey);
			void               InsertEntry            (sizet index, Seq key, ObjId objId);
			FindKeyResult::E   FindLastKeyLessThan    (Seq key, sizet& index) const;			// If not found, sets index = SIZE_MAX
			void               Split                  (ChildBucket& first, ChildBucket& second, sizet maxSize);
			void               AppendEntriesFrom      (ChildBucket& x);
			void               RemoveEntry            (sizet index);
			void               ReplaceWithEntriesFrom (ChildBucket& x);
			void               ClearEntries();

			sizet              EncodedSize            ()               const { return m_encodedSize; }
			void               Encode                 (Enc& enc) const;
			bool               Decode                 (Seq& s);

		private:
			ObjId             m_objId;
			sizet             m_encodedSize { MinimumEncodedSize };
			uint              m_height      {};
			Vec<BucketEntry>  m_entries;
			mutable bool      m_dirty       {};
		};

		struct BucketPathStep
		{
			BucketPathStep() = default;
			BucketPathStep(ChildBucket& bucket) : m_bucket(&bucket) {}

			ChildBucket* m_bucket {};
			sizet        m_index  {};
		};

		struct BucketCx : NoCopy
		{
			BucketCx(ObjId rootBucketObjId) : m_rootBucket(rootBucketObjId) {}

			ChildBucket m_rootBucket;
			RpVec<RcStr> m_bucketObjs;
			Vec<std::unique_ptr<ChildBucket>> m_buckets;
		};

		struct BucketPath
		{
			BucketPath(BucketCx& bucketCx) : m_bucketCx(bucketCx) { m_steps.Add(BucketPathStep(bucketCx.m_rootBucket)); }

			BucketCx&          GetBucketCx           ()                     { return m_bucketCx; }

			bool               Any                   () const               { return m_steps.Any(); }
			sizet              Depth                 () const               { return m_steps.Len(); }

			void               AddDeepestStep        (BucketPathStep entry) { m_steps.Add(entry); }
			void               PopDeepestStep        ()                     { m_steps.PopLast(); }

			BucketPathStep&    EntryAt               (sizet i)              { return m_steps[i]; }

			BucketPathStep&    RootStep              ()                     { return m_steps.First(); }
			ChildBucket&       RootBucket            ()                     { return *m_steps.First().m_bucket; }

			BucketPathStep&    DeepestStep           ()                     { return m_steps.Last(); }
			ChildBucket&       DeepestBucket         ()                     { return *m_steps.Last().m_bucket; }
			BucketEntry const& DeepestBucketCurEntry () const               { return (*m_steps.Last().m_bucket)[m_steps.Last().m_index]; }

			ChildBucket&       SecondDeepestBucket   ()                     { return *m_steps[m_steps.Len()-2].m_bucket; }

		private:
			BucketCx& m_bucketCx;
			VecFix<BucketPathStep, MaxPathSteps> m_steps;
		};

	private:
		ObjectStore m_objectStore;

		void InitTx                                   ();
		void ExtractRootBucket                        (Rp<RcStr> const& nodeObj, Seq& nodeBlob, ChildBucket& rootBucket);
		void LoadNextDeeperBucketInPath               (BucketPath& path);
		void ChangePathToPrecedingBucketOfSameHeight  (BucketPath& path);
		void ChangePathToSucceedingBucketOfSameHeight (BucketPath& path);
		void LoadNextLeafBucketEntryInPath            (BucketPath& path);		// If reached end, path is empty
		void LoadPrevLeafBucketEntryInPath            (BucketPath& path);		// If no previous entry, path is empty
		void ReplaceRootBucket                        (Seq parentNodeBlob, ChildBucket& rootBucket);
		void ReplaceNonRootBucket                     (ChildBucket& bucket);
		void InsertNonRootBucket                      (ChildBucket& bucket);
		void AddChildToParent                         (Node const& childNode, ObjId parentRefObjId, bool uniqueKey);
		void RemoveChildFromParent                    (Node const& childNode);
		void RemoveDeepestBucketCurEntry              (BucketPath& path);
		void RemoveAndPopDeepestBucket                (BucketPath& path);
		void UpdateDirtyBuckets                       (Seq parentNodeBlob, BucketCx& bucketCx);
	};

}
