#include "AtIncludes.h"
#include "AtTreeStore.h"

#include "AtNumCvt.h"


namespace At
{

	// TreeStore::Node

	void TreeStore::Node::Encode(Enc& enc) const
	{
		m_parentId.EncodeBin(enc);
		EncodeVarStr(enc, m_key);
		EncodeVarStr(enc, m_data);
	}


	bool TreeStore::Node::Decode(Seq& s)
	{
		Seq key, data;

		if (!m_parentId.DecodeBin(s) ||
			!DecodeVarStr(s, key) ||
			!DecodeVarStr(s, data))
			return false;

		m_key = key;
		m_data = data;
		return true;
	}


	bool TreeStore::Node::SkipDecode(Seq& s)
	{
		Seq key, data;

		if (!ObjId::SkipDecodeBin(s) ||
			!DecodeVarStr(s, key) ||
			!DecodeVarStr(s, data))
			return false;

		return true;
	}



	// TreeStore::BucketEntry

	void TreeStore::BucketEntry::Encode(Enc& enc) const
	{
		EncodeVarStr(enc, m_key);
		m_objId.EncodeBin(enc);
	}


	bool TreeStore::BucketEntry::Decode(Seq& s)
	{
		if (!DecodeVarStr(s, m_key) ||
			!m_objId.DecodeBin(s))
			return false;

		return true;
	}



	// TreeStore::ChildBucket

	void TreeStore::ChildBucket::InsertEntry(sizet index, Seq key, ObjId objId)
	{
		if (index > 0)
			EnsureAbort(key >= m_entries[index - 1].m_key);

		if (index == m_entries.Len())
			m_entries.Add();
		else
		{
			EnsureAbort(index < m_entries.Len());
			EnsureAbort(key <= m_entries[index].m_key);
			m_entries.Insert(index, BucketEntry());
		}

		m_entries[index].m_key = key;
		m_entries[index].m_objId = objId;

		m_encodedSize += m_entries[index].EncodedSize();
		m_dirty = true;
	}


	TreeStore::FindKeyResult::E TreeStore::ChildBucket::FindLastKeyLessThan(Seq key, sizet& index) const
	{
		index = SIZE_MAX;
		if (!m_entries.Any())
			return FindKeyResult::Empty;

		for (sizet i=0; i!=m_entries.Len(); ++i)
			if (m_entries[i].m_key >= key)
			{
				     if (i != 0)                   { index = i-1; return FindKeyResult::Found; }
				else if (m_entries[i].m_key > key)                return FindKeyResult::FirstKeyGreater;
				else                                              return FindKeyResult::FirstKeyEqual;
			}

		index = m_entries.Len() - 1;
		return FindKeyResult::Found;
	}


	void TreeStore::ChildBucket::ReplaceEntryKey(sizet index, Seq newKey)
	{
		EnsureAbort(index < m_entries.Len());
		if (index > 0)                   EnsureAbort(newKey >= m_entries[index - 1].m_key);
		if (index + 1 < m_entries.Len()) EnsureAbort(newKey <= m_entries[index + 1].m_key);

		m_encodedSize -= m_entries[index].EncodedSize();
		m_entries[index].m_key = newKey;
		m_encodedSize += m_entries[index].EncodedSize();

		m_dirty = true;
	}


	void TreeStore::ChildBucket::Split(ChildBucket& first, ChildBucket& second, sizet maxSize)
	{
		EnsureAbort(m_entries.Len() >= 2);
		EnsureAbort(!first.m_entries.Any());
		EnsureAbort(!second.m_entries.Any());

		first.m_height = m_height;
		second.m_height = m_height;

		auto transferEntry = [this] (ChildBucket& x, sizet i)
			{ x.m_encodedSize += x.m_entries.Add(m_entries[i]).EncodedSize(); };

		sizet i;

		for (i=0; i!=m_entries.Len(); ++i)
		{
			// Split the buckets approximately in two according to encoded size,
			// but make sure neither bucket exceeds maxSize
		
			if (first.m_encodedSize + m_entries[i].EncodedSize() > maxSize)
				break;

			transferEntry(first, i);

			if (first.m_encodedSize >= maxSize / 2)
			{
				++i;
				break;
			}
		}
	
		for (; i!=m_entries.Len(); ++i)
			transferEntry(second, i);

		EnsureAbort(first.m_entries.Len() + second.m_entries.Len() == m_entries.Len());
		EnsureAbort(second.m_encodedSize <= maxSize);

		first.m_dirty = true;
		second.m_dirty = true;

		ClearEntries();
		++m_height;
	}


	void TreeStore::ChildBucket::AppendEntriesFrom(ChildBucket& x)
	{
		if (!x.m_entries.Any())
			return;

		if (m_entries.Any())
			EnsureAbort(m_entries.Last().m_key <= x.m_entries.First().m_key);

		for (BucketEntry& entry : x.m_entries)
			m_encodedSize += m_entries.Add(entry).EncodedSize();

		x.ClearEntries();
		m_dirty = true;
	}


	void TreeStore::ChildBucket::RemoveEntry(sizet index)
	{
		EnsureAbort(index < m_entries.Len());
		m_encodedSize -= m_entries[index].EncodedSize();
		EnsureAbort(m_encodedSize >= MinimumEncodedSize);
		EnsureAbort(m_encodedSize < (SIZE_MAX - (SIZE_MAX/10)));
		m_entries.Erase(index, 1);
		m_dirty = true;
	}


	void TreeStore::ChildBucket::ReplaceWithEntriesFrom(ChildBucket& x)
	{
		m_height = x.m_height;
		m_entries.Resize(x.m_entries.Len());
		for (sizet i=0; i!=m_entries.Len(); ++i)
			m_entries[i] = x.m_entries[i];
		m_encodedSize = x.m_encodedSize;
		m_dirty = true;
	}


	void TreeStore::ChildBucket::ClearEntries()
	{
		if (m_entries.Any())
		{
			m_entries.Free();
			m_encodedSize = MinimumEncodedSize;
			m_dirty = true;
		}
	}


	void TreeStore::ChildBucket::Encode(Enc& enc) const
	{
		EncodeByte(enc, (byte) m_height);
		EncodeUInt32(enc, NumCast<uint32>(m_entries.Len()));
		for (sizet i=0; i!=m_entries.Len(); ++i)
			m_entries[i].Encode(enc);
		m_dirty = false;
	}


	bool TreeStore::ChildBucket::Decode(Seq& s)
	{
		Seq origStr = s;

		uint32 n;
		if (!DecodeByte(s, m_height) ||
			!DecodeUInt32(s, n))
			return false;

		m_entries.Resize(n);
		for (uint32 i=0; i!=n; ++i)
			if (!m_entries[i].Decode(s))
				return false;

		m_encodedSize = (sizet) (s.p - origStr.p);
		m_dirty = false;
		return true;
	}



	// TreeStore

	Str const TreeStore::KeyBeyondLast = Str().Bytes(TreeStore::MaxKeySize + 1, 0xFF);


	void TreeStore::Init()
	{
		m_objectStore.Init();
		m_objectStore.RunTxExclusive( [&] () { InitTx(); } );
	}


	void TreeStore::InsertNode(Node& node, ObjId parentRefObjId, bool uniqueKey)
	{
		EnsureThrow(node.m_key.Len() <= MaxKeySize);
		EnsureThrow(node.m_parentId != ObjId::None);

		// Insert node
		sizet       nodeSize    { node.EncodedSize() };
		ChildBucket childBucket { ObjId::None };

		if (nodeSize < ExternalNodeSizeThreshold)
		{
			// Node is small enough, can be stored internally
			Rp<RcStr> nodeObj = new RcStr;

			{
				Enc& enc = nodeObj.Ref();
				Enc::Meter meter = enc.FixMeter(1 + nodeSize + childBucket.EncodedSize());
	
				EncodeByte(enc, NodeType::Internal);
				node.Encode(enc);
				childBucket.Encode(enc);
				EnsureAbort(meter.Met());
			}

			node.m_nodeId = m_objectStore.InsertObject(nodeObj);
			node.m_contentObjId = node.m_nodeId;
		}
		else
		{
			// Node is too large, must be stored in a separate object 
			Rp<RcStr> contentObj = new RcStr;

			{
				Enc& enc = contentObj.Ref();
				Enc::Meter meter = enc.FixMeter(node.EncodedSize());
				node.Encode(enc);
				EnsureAbort(meter.Met());
			}

			node.m_contentObjId = m_objectStore.InsertObject(contentObj);

			Rp<RcStr> nodeObj { new RcStr };

			{
				Enc& enc = nodeObj.Ref();
				Enc::Meter meter = enc.FixMeter(1 + ObjId::EncodedSize + childBucket.EncodedSize());
				EncodeByte(enc, NodeType::External);
				node.m_contentObjId.EncodeBin(enc);
				childBucket.Encode(enc);
				EnsureAbort(meter.Met());
			}

			node.m_nodeId = m_objectStore.InsertObject(nodeObj);
		}

		AddChildToParent(node, parentRefObjId, uniqueKey);
	}


	bool TreeStore::GetNodeById(Node& node, ObjId refObjId)
	{
		Rp<RcStr> nodeObj { m_objectStore.RetrieveObject(node.m_nodeId, refObjId) };
		if (!nodeObj.Any())
			return false;

		Seq nodeStr { nodeObj.Ref() };
	
		uint nodeType;
		EnsureAbort(DecodeByte(nodeStr, nodeType));

		if (nodeType == NodeType::Internal)
		{
			node.m_contentObjId = node.m_nodeId;
			EnsureAbort(node.Decode(nodeStr));
		}
		else
		{
			EnsureAbort(node.m_contentObjId.DecodeBin(nodeStr));

			Rp<RcStr> contentObj { m_objectStore.RetrieveObject(node.m_contentObjId, node.m_nodeId) };
			EnsureAbort(contentObj.Any());

			Seq contentStr { contentObj.Ref() };
			EnsureAbort(node.Decode(contentStr));
		}

		ChildBucket rootBucket { node.m_nodeId };
		EnsureAbort(rootBucket.Decode(nodeStr));
		node.m_hasChildren = rootBucket.Any();
		return true;
	}


	void TreeStore::FindChildren_Forward(ObjId parentId, Seq keyFirst, Seq keyBeyondLast, std::function<bool (Seq, ObjId, ObjId)> onMatch)
	{
		if (keyBeyondLast == keyFirst)
			return;

		// Retrieve parent node
		Rp<RcStr> parentObj = m_objectStore.RetrieveObject(parentId, ObjId::None);
		if (!parentObj.Any())
			return;

		Seq parentNodeBlob;
		BucketCx bucketCx = parentId;
		ExtractRootBucket(parentObj, parentNodeBlob, bucketCx.m_rootBucket);
		if (!bucketCx.m_rootBucket.Any())
			return;

		// Find path to leaf bucket for first key
		BucketPath path = bucketCx;
		while (true)
		{
			BucketPathStep& step = path.DeepestStep();
			ChildBucket const& bucket = path.DeepestBucket();

			FindKeyResult::E fkr = bucket.FindLastKeyLessThan(keyFirst, step.m_index);
			if (fkr != FindKeyResult::Found)
				step.m_index = 0;

			if (bucket.Height() == 0)
				break;

			// Not yet at leaf bucket, search deeper
			EnsureAbort(fkr != FindKeyResult::Empty);
			LoadNextDeeperBucketInPath(path);
		}

		// Found starting leaf bucket. Enumerate matches 
		bool foundMatchStart {};
		while (path.Any())
		{
			BucketPathStep& step = path.DeepestStep();
			ChildBucket const& bucket = path.DeepestBucket();

			BucketEntry const& bucketEntry = bucket[step.m_index];
			if (bucketEntry.m_key >= keyBeyondLast)
				break;

			if (!foundMatchStart && bucketEntry.m_key >= keyFirst)
				foundMatchStart = true;
			if (foundMatchStart)
				if (!onMatch(bucketEntry.m_key, bucketEntry.m_objId, step.m_bucket->GetObjId()))
					return;

			LoadNextLeafBucketEntryInPath(path);
		}
	}


	void TreeStore::FindChildren_Reverse(ObjId parentId, Seq keyFirst, Seq keyBeyondLast, std::function<bool (Seq, ObjId, ObjId)> onMatch)
	{
		if (keyBeyondLast == keyFirst)
			return;

		// Retrieve parent node
		Rp<RcStr> parentObj = m_objectStore.RetrieveObject(parentId, ObjId::None);
		if (!parentObj.Any())
			return;

		Seq parentNodeBlob;
		BucketCx bucketCx { parentId };
		ExtractRootBucket(parentObj, parentNodeBlob, bucketCx.m_rootBucket);
		if (!bucketCx.m_rootBucket.Any())
			return;

		// Find path to leaf bucket for last key before specified
		BucketPath path = bucketCx;
		while (true)
		{
			BucketPathStep& step = path.DeepestStep();
			ChildBucket const& bucket = path.DeepestBucket();

			FindKeyResult::E fkr = bucket.FindLastKeyLessThan(keyBeyondLast, step.m_index);
			if (fkr != FindKeyResult::Found)
				return;
			
			if (bucket.Height() == 0)
				break;

			// Not yet at leaf bucket, search deeper
			EnsureAbort(fkr != FindKeyResult::Empty);
			LoadNextDeeperBucketInPath(path);
		}

		// Found last leaf bucket. Enumerate matches
		while (path.Any())
		{
			BucketPathStep& step = path.DeepestStep();
			ChildBucket const& bucket = path.DeepestBucket();

			BucketEntry const& bucketEntry = bucket[step.m_index];
			if (bucketEntry.m_key < keyFirst)
				break;

			if (!onMatch(bucketEntry.m_key, bucketEntry.m_objId, step.m_bucket->GetObjId()))
				return;

			LoadPrevLeafBucketEntryInPath(path);
		}
	}


	void TreeStore::ReplaceNode(Node& node, bool uniqueKey)
	{
		// Read original node. Node must exist. If it has been removed, it must have been previously read by this transaction,
		// so that RetrieveObject will throw RetryTxException instead of returning null.
		Node origNode;
		origNode.m_nodeId = node.m_nodeId;

		Rp<RcStr> origNodeObj { m_objectStore.RetrieveObject(origNode.m_nodeId, ObjId::None) };
		EnsureAbort(origNodeObj.Any());

		Seq origNodeStr { origNodeObj.Ref() };	
		uint origNodeType;
		EnsureAbort(DecodeByte(origNodeStr, origNodeType));

		bool haveExternalNode {};
	
		if (origNodeType == NodeType::Internal)
		{
			node.m_contentObjId = node.m_nodeId;
			EnsureAbort(origNode.Decode(origNodeStr));
		}
		else
		{
			haveExternalNode = true;
			EnsureAbort(node.m_contentObjId.DecodeBin(origNodeStr));
		
			Rp<RcStr> contentObj { m_objectStore.RetrieveObject(node.m_contentObjId, node.m_nodeId) };
			EnsureAbort(contentObj.Any());
	
			Seq contentStr { contentObj.Ref() };
			EnsureAbort(origNode.Decode(contentStr));
		}

		// Write new node
		sizet nodeSize { node.EncodedSize() };
		if (nodeSize < ExternalNodeSizeThreshold)
		{
			ChildBucket rootBucket { node.m_nodeId };
			EnsureAbort(rootBucket.Decode(origNodeStr));

			// If there's an external node, remove it
			if (haveExternalNode)
			{
				m_objectStore.RemoveObject(node.m_contentObjId, node.m_nodeId);
				node.m_contentObjId = node.m_nodeId;
			}

			// Write internal node
			Rp<RcStr> newNodeObj = new RcStr;

			{
				Enc& enc = newNodeObj.Ref();
				Enc::Meter meter = enc.FixMeter(1 + nodeSize + rootBucket.EncodedSize());
				EncodeByte(enc, NodeType::Internal);
				node.Encode(enc);
				rootBucket.Encode(enc);
				EnsureAbort(meter.Met());
			}

			m_objectStore.ReplaceObject(node.m_nodeId, ObjId::None, newNodeObj);
		}
		else
		{
			// Write the external node
			Rp<RcStr> newContentObj { new RcStr };

			{
				Enc& enc = newContentObj.Ref();
				Enc::Meter meter = enc.FixMeter(nodeSize);
				node.Encode(enc);
				EnsureAbort(meter.Met());
			}

			if (haveExternalNode)
			{
				// Replace existing external node
				m_objectStore.ReplaceObject(node.m_contentObjId, node.m_nodeId, newContentObj);
			}
			else
			{
				ChildBucket rootBucket { node.m_nodeId };
				EnsureAbort(rootBucket.Decode(origNodeStr));

				// Create new external node
				node.m_contentObjId = m_objectStore.InsertObject(newContentObj);

				// Write the main node
				Rp<RcStr> newNodeObj { new RcStr };

				{
					Enc& enc = newNodeObj.Ref();
					Enc::Meter meter = enc.FixMeter(1 + ObjId::EncodedSize + rootBucket.EncodedSize());
					EncodeByte(enc, NodeType::External);
					node.m_contentObjId.EncodeBin(enc);
					rootBucket.Encode(enc);
					EnsureAbort(meter.Met());
				}

				m_objectStore.ReplaceObject(node.m_nodeId, ObjId::None, newNodeObj);
			}
		}
	
		// Update parent reference, if necessary
		if (node.m_nodeId != ObjId::Root && (node.m_key != origNode.m_key || node.m_parentId != origNode.m_parentId))
		{
			if (node.m_parentId != origNode.m_parentId)
			{
				// Ensure that a cycle is not being created
				ObjId ancestorId = node.m_parentId;
				ObjId refId = node.m_nodeId;

				while (ancestorId != ObjId::Root)
				{
					EnsureAbort(ancestorId != node.m_nodeId);
				
					Node ancestor;
					ancestor.m_nodeId = ancestorId;
					EnsureAbort(GetNodeById(ancestor, refId));
					ancestorId = ancestor.m_parentId;
					refId = ancestorId;
				}
			}

			RemoveChildFromParent(origNode);
			AddChildToParent(node, ObjId::None, uniqueKey);
		}
	}


	ChildCount TreeStore::RemoveNodeChildren(ObjId nodeId)
	{
		// Enumerate node's children
		ChildCount childCount;
		Vec<ObjIdWithRef> removeNodes;
		auto addNodeToRemove = [&] (Seq, ObjId objId, ObjId bucketId) -> bool { removeNodes.Add(objId, bucketId); return true; };

		removeNodes.Add(nodeId, ObjId::None);
		EnumAllChildren(nodeId, addNodeToRemove);
		childCount.m_nrDirectChildren = removeNodes.Len() - 1;

		for (sizet i=1; i!=removeNodes.Len(); ++i)
			EnumAllChildren(removeNodes[i].m_objId, addNodeToRemove);
		childCount.m_nrAllDescendants = removeNodes.Len() - 1;

		// Remove node's children
		for (sizet i=0; i!=removeNodes.Len(); ++i)
		{
			ObjIdWithRef const& rn { removeNodes[i] };

			// Read node to remove
			Node node;
			node.m_nodeId = rn.m_objId;

			Rp<RcStr> nodeObj { m_objectStore.RetrieveObject(node.m_nodeId, rn.m_refObjId) };
			EnsureAbort(nodeObj.Any());

			Seq nodeStr { nodeObj.Ref() };	
			uint nodeType;
			EnsureAbort(DecodeByte(nodeStr, nodeType));

			bool haveExternalNode {};

			if (nodeType == NodeType::Internal)
			{
				EnsureAbort(node.Decode(nodeStr));
				node.m_contentObjId = node.m_nodeId;
			}
			else
			{
				haveExternalNode = true;
				EnsureAbort(node.m_contentObjId.DecodeBin(nodeStr));
			}

			BucketCx bucketCx { rn.m_objId };
			EnsureAbort(bucketCx.m_rootBucket.Decode(nodeStr));

			// Walk and remove child buckets
			BucketPath path { bucketCx };
		
			while (path.DeepestBucket().Height() > 0)
				LoadNextDeeperBucketInPath(path);

			while (path.Depth() > 1)
			{
				BucketPath curLevelPath { path };

				do
				{
					ObjId removeBucketId { curLevelPath.DeepestBucket().GetObjId() };
					ChangePathToSucceedingBucketOfSameHeight(curLevelPath);
					m_objectStore.RemoveObject(removeBucketId, ObjId::None);
				}
				while (curLevelPath.Any());	

				path.PopDeepestStep();
			}

			if (i > 0)
			{
				// Remove node
				if (haveExternalNode)
					m_objectStore.RemoveObject(node.m_contentObjId, ObjId::None);

				m_objectStore.RemoveObject(rn.m_objId, ObjId::None);
			}
			else
			{
				// Update node with cleared root bucket
				bucketCx.m_rootBucket.ClearEntries();

				Rp<RcStr> newNodeObj(new RcStr);

				sizet nodeSize    { node.EncodedSize() };
				sizet newNodeSize { 1 +
					                If(haveExternalNode, sizet, ObjId::EncodedSize, nodeSize) +
					                bucketCx.m_rootBucket.EncodedSize() };

				{
					Enc& enc = newNodeObj.Ref();
					Enc::Meter meter = enc.FixMeter(newNodeSize);
					EncodeByte(enc, (byte) nodeType);

					if (haveExternalNode)
						node.m_contentObjId.EncodeBin(enc);
					else
						node.Encode(enc);

					bucketCx.m_rootBucket.Encode(enc);
					EnsureAbort(meter.Met());
				}

				m_objectStore.ReplaceObject(nodeId, ObjId::None, newNodeObj);
			}
		}

		return childCount;
	}


	void TreeStore::RemoveNode(ObjId nodeId)
	{
		EnsureAbort(nodeId != ObjId::Root);

		Node node;
		node.m_nodeId = nodeId;

		Rp<RcStr> nodeObj { m_objectStore.RetrieveObject(node.m_nodeId, ObjId::None) };
		if (!nodeObj.Any())
			return;

		// Decode node
		Seq nodeStr { nodeObj.Ref() };
	
		uint nodeType;
		EnsureAbort(DecodeByte(nodeStr, nodeType));

		bool haveExternalNode {};

		if (nodeType == NodeType::Internal)
		{
			EnsureAbort(node.Decode(nodeStr));
			node.m_contentObjId = node.m_nodeId;
		}
		else
		{
			haveExternalNode = true;
			EnsureAbort(node.m_contentObjId.DecodeBin(nodeStr));

			Rp<RcStr> contentObj { m_objectStore.RetrieveObject(node.m_contentObjId, node.m_nodeId) };
			EnsureAbort(contentObj.Any());

			Seq contentStr { contentObj.Ref() };
			EnsureAbort(node.Decode(contentStr));
		}

		ChildBucket rootBucket { node.m_nodeId };
		EnsureAbort(rootBucket.Decode(nodeStr));

		// Node to be removed must not have children
		EnsureAbort(!rootBucket.Any());

		// Remove node
		if (haveExternalNode)
			m_objectStore.RemoveObject(node.m_contentObjId, ObjId::None);

		m_objectStore.RemoveObject(node.m_nodeId, ObjId::None);
		RemoveChildFromParent(node);
	}



	// Private methods

	void TreeStore::InitTx()
	{
		Rp<RcStr> rootObj { m_objectStore.RetrieveObject(ObjId::Root, ObjId::None) };
		if (!rootObj.Any())
		{
			// No root object in database yet.
			// Insert empty root object.

			Node rootNode;
			ChildBucket childBucket { ObjId::None };

			rootObj.Set(new RcStr);

			{
				Enc& enc = rootObj.Ref();
				Enc::Meter meter = enc.FixMeter(1 + rootNode.EncodedSize() + childBucket.EncodedSize());
				EncodeByte(enc, NodeType::Internal);
				rootNode.Encode(enc);
				childBucket.Encode(enc);
				EnsureAbort(meter.Met());
			}		

			ObjId insertedId = m_objectStore.InsertObject(rootObj);
			EnsureAbort(insertedId == ObjId::Root);
		}
	}


	void TreeStore::ExtractRootBucket(Rp<RcStr> const& nodeObj, Seq& nodeBlob, ChildBucket& rootBucket)
	{
		Seq nodeStr(nodeObj.Ref());
		Seq origNodeStr = nodeStr;

		{
			uint nodeType;
			EnsureAbort(DecodeByte(nodeStr, nodeType));

			if (nodeType == NodeType::Internal) EnsureAbort(Node::SkipDecode(nodeStr));
			else                                EnsureAbort(ObjId::SkipDecodeBin(nodeStr));

			nodeBlob = Seq(origNodeStr.p, (sizet) (nodeStr.p - origNodeStr.p));
		}
	
		EnsureAbort(rootBucket.Decode(nodeStr));
	}


	void TreeStore::LoadNextDeeperBucketInPath(BucketPath& path)
	{
		ObjId bucketObjId { path.DeepestBucketCurEntry().m_objId };

		// See if the bucket is already loaded
		BucketCx& bucketCx { path.GetBucketCx() };
		for (std::unique_ptr<ChildBucket>& childBucket : bucketCx.m_buckets)
			if (childBucket->GetObjId() == bucketObjId)
			{
				path.AddDeepestStep(BucketPathStep(*childBucket));
				return;
			}

		// Load bucket
		Rp<RcStr> bucketObj { m_objectStore.RetrieveObject(bucketObjId, path.DeepestBucket().GetObjId()) };
		EnsureAbort(bucketObj.Any());
		Seq bucketStr { bucketObj.Ref() };

		bucketCx.m_bucketObjs.Add(bucketObj);
		ChildBucket& bucket = *bucketCx.m_buckets.Add(std::make_unique<ChildBucket>(bucketObjId));
		EnsureAbort(bucket.Decode(bucketStr));
		EnsureAbort(bucket.Any());		// Non-root bucket must not be empty
		path.AddDeepestStep(BucketPathStep(bucket));
	}


	void TreeStore::ChangePathToPrecedingBucketOfSameHeight(BucketPath& path)
	{
		sizet depth = path.Depth();

		// Go up as far as necessary
		do
			path.PopDeepestStep();
		while (path.Any() && path.DeepestStep().m_index == 0);

		if (!path.Any())
			return;			// There is no preceding sibling. Result is null path

		--(path.DeepestStep().m_index);

		// Go down to preceding bucket on same level
		do
		{
			LoadNextDeeperBucketInPath(path);
			EnsureAbort(path.DeepestBucket().Any());
			path.DeepestStep().m_index = path.DeepestBucket().NrEntries() - 1;
		}
		while (path.Depth() != depth);
	}


	void TreeStore::ChangePathToSucceedingBucketOfSameHeight(BucketPath& path)
	{
		sizet depth = path.Depth();

		// Go up as far as necessary
		do
			path.PopDeepestStep();
		while (path.Any() && path.DeepestStep().m_index == path.DeepestBucket().NrEntries() - 1);

		if (!path.Any())
			return;			// There is no succeeding sibling. Result is null path

		++(path.DeepestStep().m_index);

		// Go down to succeeding bucket on same level
		do
			LoadNextDeeperBucketInPath(path);
		while (path.Depth() != depth);
	}


	void TreeStore::LoadNextLeafBucketEntryInPath(BucketPath& path)
	{
		EnsureAbort(path.DeepestBucket().Height() == 0);

		while (++(path.DeepestStep().m_index) >= path.DeepestBucket().NrEntries())
		{
			// We're at end of current bucket. Go up a level and find the next bucket 
			path.PopDeepestStep();
			if (!path.Any())
				return;
		}

		while (path.DeepestBucket().Height() > 0)
			LoadNextDeeperBucketInPath(path);
	}


	void TreeStore::LoadPrevLeafBucketEntryInPath(BucketPath& path)
	{
		EnsureAbort(path.DeepestBucket().Height() == 0);

		while (path.DeepestStep().m_index == 0)
		{
			// We're at end of current bucket. Go up a level and find the next bucket 
			path.PopDeepestStep();
			if (!path.Any())
				return;
		}

		--(path.DeepestStep().m_index);

		if (path.DeepestBucket().Height() > 0)
		{
			while (path.DeepestBucket().Height() > 0)
				LoadNextDeeperBucketInPath(path);

			sizet nrEntries = path.DeepestBucket().NrEntries();
			EnsureAbort(nrEntries);
			path.DeepestStep().m_index = nrEntries - 1;
		}
	}


	void TreeStore::ReplaceRootBucket(Seq nodeBlob, ChildBucket& rootBucket)
	{
		EnsureAbort(rootBucket.GetObjId() != ObjId::None);

		Rp<RcStr> obj { new RcStr };

		{
			Enc& enc = obj.Ref();
			Enc::Meter meter = enc.FixMeter(nodeBlob.n + rootBucket.EncodedSize());
			EncodeVerbatim(enc, nodeBlob);
			rootBucket.Encode(enc);
			EnsureAbort(meter.Met());
		}

		m_objectStore.ReplaceObject(rootBucket.GetObjId(), ObjId::None, obj);
	}


	void TreeStore::ReplaceNonRootBucket(ChildBucket& bucket)
	{
		EnsureAbort(bucket.GetObjId() != ObjId::None);
		EnsureAbort(bucket.Any());		// Non-root bucket must not be empty

		Rp<RcStr> bucketObj { new RcStr };

		{
			Enc& enc = bucketObj.Ref();
			Enc::Meter meter = enc.FixMeter(bucket.EncodedSize());
			bucket.Encode(enc);
			EnsureAbort(meter.Met());
		}

		m_objectStore.ReplaceObject(bucket.GetObjId(), ObjId::None, bucketObj);
	}


	void TreeStore::InsertNonRootBucket(ChildBucket& bucket)
	{
		EnsureAbort(bucket.GetObjId() == ObjId::None);

		Rp<RcStr> bucketObj(new RcStr);

		{
			Enc& enc = bucketObj.Ref();
			Enc::Meter meter = enc.FixMeter(bucket.EncodedSize());
			bucket.Encode(enc);
			EnsureAbort(meter.Met());
		}

		bucket.SetObjId(m_objectStore.InsertObject(bucketObj));
	};


	void TreeStore::AddChildToParent(Node const& childNode, ObjId parentRefObjId, bool uniqueKey)
	{
		// Retrieve parent node. Parent node must exist, or if it has been removed, it must have been previously read,
		// or the reference object must have changed, so that RetrieveObject will throw RetryTxException instead of returning null.
		Rp<RcStr> parentObj { m_objectStore.RetrieveObject(childNode.m_parentId, parentRefObjId) };
		EnsureAbort(parentObj.Any());

		Seq parentNodeBlob;
		BucketCx bucketCx { childNode.m_parentId };
		ExtractRootBucket(parentObj, parentNodeBlob, bucketCx.m_rootBucket);

		// Find bucket to which this child should be added
		BucketPath path { bucketCx };
		FindKeyResult::E fkr;

		while (true)
		{
			BucketPathStep& step { path.DeepestStep() };
			fkr = step.m_bucket->FindLastKeyLessThan(childNode.m_key, step.m_index);
			if (path.DeepestBucket().Height() == 0)
				break;

			if (fkr != FindKeyResult::Found)
			{
				EnsureAbort(fkr != FindKeyResult::Empty);		// Must not be empty because not a leaf bucket
				if (fkr == FindKeyResult::FirstKeyGreater)
					step.m_bucket->ReplaceEntryKey(0, childNode.m_key);

				step.m_index = 0;
			}

			// Not yet at leaf bucket, search deeper
			LoadNextDeeperBucketInPath(path);
		}

		// Found leaf bucket. If necessary, verify that key is unique
		if (uniqueKey)
		{
			// If a unique key violation is detected at this point, we can't throw a nicer exception type.
			// The object store was modified in InsertNode or ReplaceNode. There is no way to recover without aborting the transaction.
			// To permit recovery, the application must detect the conflict before trying to modify the database in error.
			EnsureThrow(fkr != FindKeyResult::FirstKeyEqual);
			if (fkr == FindKeyResult::Found)
			{
				BucketPath uniqueCheckPath { path };
				LoadNextLeafBucketEntryInPath(uniqueCheckPath);
				if (uniqueCheckPath.Any())
					EnsureThrow(childNode.m_key < uniqueCheckPath.DeepestBucketCurEntry().m_key);
			}
		}
		
		// Start by updating the leaf bucket, then update buckets to the top
		bool  haveObjIdToAdd { true };
		Seq   keyToAdd       { childNode.m_key };
		ObjId objIdToAdd     { childNode.m_nodeId };

		do
		{
			if (haveObjIdToAdd)
			{
				sizet index { path.DeepestStep().m_index };
				if (index == SIZE_MAX) path.DeepestBucket().InsertEntry(0,         keyToAdd, objIdToAdd);
				else                   path.DeepestBucket().InsertEntry(index + 1, keyToAdd, objIdToAdd);
			}

			if (path.DeepestBucket().Dirty())
			{
				if (path.Depth() == 1)
				{
					// Updating root bucket
					haveObjIdToAdd = false;

					if (bucketCx.m_rootBucket.EncodedSize() > MaxRootBucketSize)
					{
						// Root bucket must be split
						ChildBucket split1 { ObjId::None }, split2 { ObjId::None };
						bucketCx.m_rootBucket.Split(split1, split2, MaxRootBucketSize);
						InsertNonRootBucket(split1);
						InsertNonRootBucket(split2);

						// Add entries to new root bucket
						bucketCx.m_rootBucket.InsertEntry(0, split1[0].m_key, split1.GetObjId());
						bucketCx.m_rootBucket.InsertEntry(1, split2[0].m_key, split2.GetObjId());
					}

					ReplaceRootBucket(parentNodeBlob, bucketCx.m_rootBucket);
				}
				else
				{
					// Updating non-root bucket
					if (path.DeepestBucket().EncodedSize() < MaxNonRootBucketSize)
					{
						ReplaceNonRootBucket(path.DeepestBucket());
						haveObjIdToAdd = false;
					}
					else
					{
						// Bucket must be split
						ChildBucket split1 { path.DeepestBucket().GetObjId() }, split2 { ObjId::None };
						path.DeepestBucket().Split(split1, split2, MaxNonRootBucketSize);
						ReplaceNonRootBucket(split1);
						InsertNonRootBucket(split2);
						keyToAdd = split2[0].m_key;
						objIdToAdd = split2.GetObjId();
					}
				}
			}

			path.PopDeepestStep();
		}
		while (path.Any());
	}


	void TreeStore::RemoveChildFromParent(Node const& childNode)
	{
		// Retrieve parent node
		Rp<RcStr> parentObj { m_objectStore.RetrieveObject(childNode.m_parentId, childNode.m_nodeId) };
		EnsureAbort(parentObj.Any());	// Parent node must be successfully retrieved if child node exists

		Seq parentNodeBlob;
		BucketCx bucketCx(childNode.m_parentId);
		ExtractRootBucket(parentObj, parentNodeBlob, bucketCx.m_rootBucket);

		// Find bucket containing this child
		BucketPath path(bucketCx);

		while (true)
		{
			BucketPathStep& step = path.DeepestStep();
			FindKeyResult::E fkr = step.m_bucket->FindLastKeyLessThan(childNode.m_key, step.m_index);
			if (fkr == FindKeyResult::FirstKeyEqual)
				step.m_index = 0;
			else
				EnsureAbort(step.m_index != SIZE_MAX);

			if (step.m_bucket->Height() == 0)
				break;

			// Not yet at leaf bucket, search deeper
			LoadNextDeeperBucketInPath(path);
		}

		while (path.DeepestBucketCurEntry().m_objId != childNode.m_nodeId)
		{
			EnsureAbort(path.DeepestBucketCurEntry().m_key <= Seq(childNode.m_key));
			LoadNextLeafBucketEntryInPath(path);
			EnsureAbort(path.Any());
		}

		EnsureAbort(path.DeepestBucketCurEntry().m_key == Seq(childNode.m_key));
		RemoveDeepestBucketCurEntry(path);

		UpdateDirtyBuckets(parentNodeBlob, bucketCx);
	}


	void TreeStore::RemoveDeepestBucketCurEntry(BucketPath& path)
	{
		Seq removedKey(path.DeepestBucketCurEntry().m_key);
		path.DeepestBucket().RemoveEntry(path.DeepestStep().m_index);

		if (!path.DeepestBucket().Any())
		{
			// No more entries in bucket
			if (path.Depth() > 1)
			{
				// Bucket is non-root, and is now empty. Delete it, then proceed to remove its entry from parent bucket
				RemoveAndPopDeepestBucket(path);
				RemoveDeepestBucketCurEntry(path);
			}
		}
		else if (path.Depth() == 1)
		{
			// Is root bucket
			if (path.RootBucket().NrEntries() == 1 && path.RootBucket().Height() > 0)
			{
				// Root bucket now has only one child bucket. Replace root bucket with first deeper bucket having more than one entry
				path.RootStep().m_index = 0;

				do
					LoadNextDeeperBucketInPath(path);
				while (path.DeepestBucket().NrEntries() == 1 && path.DeepestBucket().Height() > 0);

				path.RootBucket().ReplaceWithEntriesFrom(path.DeepestBucket());

				do
					RemoveAndPopDeepestBucket(path);
				while (path.Depth() > 1);
			}
		}
		else
		{
			// If we removed the first bucket entry, update key information in parent buckets
			if (path.DeepestStep().m_index == 0)
			{
				BucketPath fixKeyPath(path);
				Seq newKey(path.DeepestBucketCurEntry().m_key);
			
				do
				{
					fixKeyPath.PopDeepestStep();
					EnsureAbort(fixKeyPath.DeepestBucketCurEntry().m_key == removedKey);
					fixKeyPath.DeepestBucket().ReplaceEntryKey(fixKeyPath.DeepestStep().m_index, newKey);
				}
				while (fixKeyPath.Depth() > 1 && fixKeyPath.DeepestStep().m_index == 0);
			}

			// Join with neighboring buckets, if possible and necessary
			sizet curBucketSize = path.DeepestBucket().EncodedSize();
			if (curBucketSize <= BucketJoinThreshold)
			{
				BucketPath prevBucketPath(path);
				ChangePathToPrecedingBucketOfSameHeight(prevBucketPath);

				BucketPath nextBucketPath(path);
				ChangePathToSucceedingBucketOfSameHeight(nextBucketPath);

				sizet prevBucketSize = If(!prevBucketPath.Any(), sizet, SIZE_MAX, prevBucketPath.DeepestBucket().EncodedSize());
				sizet nextBucketSize = If(!nextBucketPath.Any(), sizet, SIZE_MAX, nextBucketPath.DeepestBucket().EncodedSize());
				EnsureAbort(prevBucketSize != SIZE_MAX || nextBucketSize != SIZE_MAX);

				BucketPath* joinBucketPath1 = nullptr;
				BucketPath* joinBucketPath2 = nullptr;
			
				if (prevBucketSize < nextBucketSize)
				{
					if (prevBucketSize + curBucketSize <= MaxNonRootBucketSize)
					{
						// Join current bucket with previous
						joinBucketPath1 = &prevBucketPath;
						joinBucketPath2 = &path;
					}
				}
				else
				{
					if (curBucketSize + nextBucketSize <= MaxNonRootBucketSize)
					{
						// Join current bucket with next
						joinBucketPath1 = &path;
						joinBucketPath2 = &nextBucketPath;
					}
				}

				if (joinBucketPath1)
				{
					// Join buckets
					EnsureAbort(joinBucketPath2);
					joinBucketPath1->DeepestBucket().AppendEntriesFrom(joinBucketPath2->DeepestBucket());
					RemoveAndPopDeepestBucket(*joinBucketPath2);
					RemoveDeepestBucketCurEntry(*joinBucketPath2);
				}
			}
		}
	}


	void TreeStore::RemoveAndPopDeepestBucket(BucketPath& path)
	{
		ObjId objId { path.DeepestBucket().GetObjId() };
		m_objectStore.RemoveObject(objId, ObjId::None);
		path.PopDeepestStep();
				
		BucketCx& bucketCx { path.GetBucketCx() };
		sizet nrRemoved {};

		for (sizet i=0; i!=bucketCx.m_buckets.Len(); )
			if (bucketCx.m_buckets[i]->GetObjId() != objId)
				++i;
			else
			{
				bucketCx.m_buckets.Erase(i, 1);
				++nrRemoved;
			}

		EnsureAbort(nrRemoved == 1);
	}


	void TreeStore::UpdateDirtyBuckets(Seq parentNodeBlob, BucketCx& bucketCx)
	{
		if (bucketCx.m_rootBucket.Dirty())
			ReplaceRootBucket(parentNodeBlob, bucketCx.m_rootBucket);

		for (std::unique_ptr<ChildBucket>& childBucket : bucketCx.m_buckets)
			if (childBucket->Dirty())
				ReplaceNonRootBucket(*childBucket);
	}

}
