#include "AtIncludes.h"
#include "AtEntityStore.h"

namespace At
{
	// Encode

	sizet Entity::EncodeOptDyn_Size(Rp<Entity> const& o)
	{
		sizet n;
		if (!o.Any())
			n = 0;
		else
			n = EncodeVarSInt64_Size(o->GetKind()) +
				EncodeVarUInt64_Size(o->GetNrFields()) +
				o->EncodeFields_Size();
		return n;
	}


	void Entity::EncodeOptDyn(Enc& enc, Rp<Entity> const& o)
	{
		if (o.Any())
		{
			EncodeVarSInt64(enc, o->GetKind());
			EncodeVarUInt64(enc, o->GetNrFields());
			o->EncodeFields(enc);
		}
	}


	sizet Entity::EncodeVecDyn_Size(RpVec<Entity> const& v)
	{
		sizet n = EncodeVarUInt64_Size(v.Len());
		for (sizet i=0; i!=v.Len(); ++i)
			if (!v[i].Any())
				n += EncodeVarSInt64_Size(-1);
			else
				n += EncodeVarSInt64_Size(v[i]->GetKind()) +
				     EncodeVarUInt64_Size(v[i]->GetNrFields()) +
				     v[i]->EncodeFields_Size();
		return n;
	}


	void Entity::EncodeVecDyn(Enc& enc, RpVec<Entity> const& v)
	{
		EncodeVarUInt64(enc, v.Len());
		for (sizet i=0; i!=v.Len(); ++i)
			if (!v[i].Any())
				EncodeVarSInt64(enc, -1);
			else
			{
				EncodeVarSInt64(enc, v[i]->GetKind());
				EncodeVarUInt64(enc, v[i]->GetNrFields());
				v[i]->EncodeFields(enc);
			}
	}


	sizet Entity::EncodeField_Size(EntityFieldInfo const* efi, FldEncDisp::E fed) const
	{
		if (fed == FldEncDisp::AsKey)
			EnsureThrow(KeyCat::IsKey(efi->m_keyCat));

		switch (efi->m_fieldType)
		{
		case FieldType::Entity:		return EncodeVal_Size (         FieldRef<Entity>(*this, efi->m_offset), fed);
		case FieldType::Bool:		return EncodeVal_Size (         FieldRef<bool  >(*this, efi->m_offset), fed);
		case FieldType::Enum:		return EncodeVal_Size ((uint64) FieldRef<uint32>(*this, efi->m_offset), fed);
		case FieldType::UInt:		return EncodeVal_Size (         FieldRef<uint64>(*this, efi->m_offset), fed);
		case FieldType::SInt:		return EncodeVal_Size (         FieldRef<int64 >(*this, efi->m_offset), fed);
		case FieldType::Float:		return EncodeVal_Size (         FieldRef<double>(*this, efi->m_offset), fed);
		case FieldType::Time:		return EncodeVal_Size (         FieldRef<Time  >(*this, efi->m_offset), fed);
		case FieldType::ObjId:		return EncodeVal_Size (         FieldRef<ObjId >(*this, efi->m_offset), fed);
		case FieldType::Str:		return EncodeVal_Size (         FieldRef<Str   >(*this, efi->m_offset), fed);

		case FieldType::OptDyn:		return EncodeOptDyn_Size (FieldRef<Rp<Entity   >>(*this, efi->m_offset));
		case FieldType::OptEnt:		return EncodeOpt_Size    (FieldRef<EntOptBase   >(*this, efi->m_offset));
		case FieldType::OptBool:	return EncodeOpt_Size    (FieldRef<Opt<bool    >>(*this, efi->m_offset));
		case FieldType::OptUInt:	return EncodeOpt_Size    (FieldRef<Opt<uint64  >>(*this, efi->m_offset));
		case FieldType::OptSInt:	return EncodeOpt_Size    (FieldRef<Opt<int64   >>(*this, efi->m_offset));
		case FieldType::OptFloat:	return EncodeOpt_Size    (FieldRef<Opt<double  >>(*this, efi->m_offset));
		case FieldType::OptTime:	return EncodeOpt_Size    (FieldRef<Opt<Time    >>(*this, efi->m_offset));
		case FieldType::OptObjId:	return EncodeOpt_Size    (FieldRef<Opt<ObjId   >>(*this, efi->m_offset));
		case FieldType::OptStr:		return EncodeOpt_Size    (FieldRef<Opt<Str     >>(*this, efi->m_offset));

		case FieldType::VecDyn:		return EncodeVecDyn_Size (FieldRef<RpVec<Entity>>(*this, efi->m_offset));
		case FieldType::VecEnt:		return EncodeVec_Size    (FieldRef<EntVecBase   >(*this, efi->m_offset));
		case FieldType::VecBool:	return EncodeVec_Size    (FieldRef<Vec<bool    >>(*this, efi->m_offset));
		case FieldType::VecUInt:	return EncodeVec_Size    (FieldRef<Vec<uint64  >>(*this, efi->m_offset));
		case FieldType::VecSInt:	return EncodeVec_Size    (FieldRef<Vec<int64   >>(*this, efi->m_offset));
		case FieldType::VecFloat:	return EncodeVec_Size    (FieldRef<Vec<double  >>(*this, efi->m_offset));
		case FieldType::VecTime:	return EncodeVec_Size    (FieldRef<Vec<Time    >>(*this, efi->m_offset));
		case FieldType::VecObjId:	return EncodeVec_Size    (FieldRef<Vec<ObjId   >>(*this, efi->m_offset));
		case FieldType::VecStr:		return EncodeVec_Size    (FieldRef<Vec<Str     >>(*this, efi->m_offset));
																  
		default:
			EnsureThrow(!"Unrecognized field type");
			return 0;
		}
	}


	void Entity::EncodeField(Enc& enc, EntityFieldInfo const* efi, FldEncDisp::E fed) const
	{
		if (fed == FldEncDisp::AsKey)
			EnsureThrow(KeyCat::IsKey(efi->m_keyCat));

		switch (efi->m_fieldType)
		{
		case FieldType::Entity:		EncodeVal(enc,          FieldRef<Entity>(*this, efi->m_offset), fed); break;
		case FieldType::Bool:		EncodeVal(enc,          FieldRef<bool  >(*this, efi->m_offset), fed); break;
		case FieldType::Enum:		EncodeVal(enc, (uint64) FieldRef<uint32>(*this, efi->m_offset), fed); break;
		case FieldType::UInt:		EncodeVal(enc,          FieldRef<uint64>(*this, efi->m_offset), fed); break;
		case FieldType::SInt:		EncodeVal(enc,          FieldRef<int64 >(*this, efi->m_offset), fed); break;
		case FieldType::Float:		EncodeVal(enc,          FieldRef<double>(*this, efi->m_offset), fed); break;
		case FieldType::Time:		EncodeVal(enc,          FieldRef<Time  >(*this, efi->m_offset), fed); break;
		case FieldType::ObjId:		EncodeVal(enc,          FieldRef<ObjId >(*this, efi->m_offset), fed); break;

		case FieldType::Str:
		{
			Str const& v = FieldRef<Str>(*this, efi->m_offset);
			if (fed == FldEncDisp::AsKey && KeyCat::IsInsensitiveKey(efi->m_keyCat))
				EncodeVal(enc, Str().Lower(v), fed);
			else
				EncodeVal(enc, v, fed);
			break;
		}

		case FieldType::OptDyn:    EncodeOptDyn(enc, FieldRef<Rp<Entity   >>(*this, efi->m_offset)); break;
		case FieldType::OptEnt:    EncodeOpt   (enc, FieldRef<EntOptBase   >(*this, efi->m_offset)); break;
		case FieldType::OptBool:   EncodeOpt   (enc, FieldRef<Opt<bool    >>(*this, efi->m_offset)); break;
		case FieldType::OptUInt:   EncodeOpt   (enc, FieldRef<Opt<uint64  >>(*this, efi->m_offset)); break;
		case FieldType::OptSInt:   EncodeOpt   (enc, FieldRef<Opt<int64   >>(*this, efi->m_offset)); break;
		case FieldType::OptFloat:  EncodeOpt   (enc, FieldRef<Opt<double  >>(*this, efi->m_offset)); break;
		case FieldType::OptTime:   EncodeOpt   (enc, FieldRef<Opt<Time    >>(*this, efi->m_offset)); break;
		case FieldType::OptObjId:  EncodeOpt   (enc, FieldRef<Opt<ObjId   >>(*this, efi->m_offset)); break;
		case FieldType::OptStr:    EncodeOpt   (enc, FieldRef<Opt<Str     >>(*this, efi->m_offset)); break;

		case FieldType::VecDyn:    EncodeVecDyn(enc, FieldRef<RpVec<Entity>>(*this, efi->m_offset)); break;
		case FieldType::VecEnt:    EncodeVec   (enc, FieldRef<EntVecBase   >(*this, efi->m_offset)); break;
		case FieldType::VecBool:   EncodeVec   (enc, FieldRef<Vec<bool    >>(*this, efi->m_offset)); break;
		case FieldType::VecUInt:   EncodeVec   (enc, FieldRef<Vec<uint64  >>(*this, efi->m_offset)); break;
		case FieldType::VecSInt:   EncodeVec   (enc, FieldRef<Vec<int64   >>(*this, efi->m_offset)); break;
		case FieldType::VecFloat:  EncodeVec   (enc, FieldRef<Vec<double  >>(*this, efi->m_offset)); break;
		case FieldType::VecTime:   EncodeVec   (enc, FieldRef<Vec<Time    >>(*this, efi->m_offset)); break;
		case FieldType::VecObjId:  EncodeVec   (enc, FieldRef<Vec<ObjId   >>(*this, efi->m_offset)); break;
		case FieldType::VecStr:    EncodeVec   (enc, FieldRef<Vec<Str     >>(*this, efi->m_offset)); break;

		default:
			EnsureThrow(!"Unrecognized field type");
		}
	}


	sizet Entity::EncodeFields_Size() const
	{
		sizet encodedSize {};
		sizet nrOptional {};

		for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
		{
			encodedSize += EncodeField_Size(efi, FldEncDisp::AsField);
			if (FieldType::IsOptional(efi->m_fieldType))
				++nrOptional;
		}

		encodedSize += ((nrOptional + 7) / 8);
		return encodedSize;
	}


	void Entity::EncodeFields(Enc& enc) const
	{
		uint nrOptFieldsToGo {}, currentlyEncodingVersion {};
		while (true)
		{
			// We allow entity types to add new fields in the middle by encoding higher version fields after previous version fields.

			uint lowestNextVersion = UINT_MAX;
			for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
			{
				if (efi->m_version > currentlyEncodingVersion)
				{
					if (lowestNextVersion > efi->m_version)
						lowestNextVersion = efi->m_version;
				}
				else if (efi->m_version == currentlyEncodingVersion)
				{
					// The presence of optional fields is encoded with a single byte before each 0th, 8th, 16th, etc optional field.
					// The byte encodes the presence of up to 8 optional fields that follow, starting with least significant bit for first field.
					// The up to 8 optional fields whose presence is encoded by the byte can be interleaved with non-optional fields.

					if (FieldType::IsOptional(efi->m_fieldType))
					{
						if (nrOptFieldsToGo)
							--nrOptFieldsToGo;
						else
							nrOptFieldsToGo = EncodeOptFieldPresence(enc, efi, currentlyEncodingVersion, lowestNextVersion);
					}

					EncodeField(enc, efi, FldEncDisp::AsField);
				}
			}

			if (lowestNextVersion == UINT_MAX)
				break;

			currentlyEncodingVersion = lowestNextVersion;
		}
	}


	uint Entity::EncodeOptFieldPresence(Enc& enc, EntityFieldInfo const* efi, uint currentlyEncodingVersion, uint lowestNextVersion) const
	{
		// Look ahead at up to 8 optional fields we're still going to encode and encode a byte indicating which ones are present.
		// This task is complicated by that fields of higher versions can appear earlier in m_fields but are to be encoded later.

		uint nrOptFieldsFound {};
		byte optFieldPresence {};

		while (true)
		{
			for (; !efi->IsPastEnd(); ++efi)
			{
				if (efi->m_version > currentlyEncodingVersion)
				{
					if (lowestNextVersion > efi->m_version)
						lowestNextVersion = efi->m_version;
				}
				else if (efi->m_version == currentlyEncodingVersion)
				{
					bool isOptField {}, isPresent {};
					switch (efi->m_fieldType)
					{
					case FieldType::OptDyn:    isOptField = true; isPresent = FieldRef<Rp<Entity >>(*this, efi->m_offset).Any(); break;
					case FieldType::OptEnt:    isOptField = true; isPresent = FieldRef<EntOptBase >(*this, efi->m_offset).Any(); break;
					case FieldType::OptBool:   isOptField = true; isPresent = FieldRef<Opt<bool  >>(*this, efi->m_offset).Any(); break;
					case FieldType::OptUInt:   isOptField = true; isPresent = FieldRef<Opt<uint64>>(*this, efi->m_offset).Any(); break;
					case FieldType::OptSInt:   isOptField = true; isPresent = FieldRef<Opt<int64 >>(*this, efi->m_offset).Any(); break;
					case FieldType::OptFloat:  isOptField = true; isPresent = FieldRef<Opt<double>>(*this, efi->m_offset).Any(); break;
					case FieldType::OptTime:   isOptField = true; isPresent = FieldRef<Opt<Time  >>(*this, efi->m_offset).Any(); break;
					case FieldType::OptObjId:  isOptField = true; isPresent = FieldRef<Opt<ObjId >>(*this, efi->m_offset).Any(); break;
					case FieldType::OptStr:    isOptField = true; isPresent = FieldRef<Opt<Str   >>(*this, efi->m_offset).Any(); break;
					}

					if (isOptField)
					{
						if (isPresent)
							optFieldPresence |= (byte) (1U << nrOptFieldsFound);

						if (++nrOptFieldsFound == 8)
							goto Done;
					}
				}
			}

			if (lowestNextVersion == UINT_MAX)
				break;

			currentlyEncodingVersion = lowestNextVersion;
			lowestNextVersion = 0;
			efi = m_fields;
		}

	Done:
		EncodeByte(enc, optFieldPresence);
		return nrOptFieldsFound;
	}


	void Entity::EncodeKey(Enc& enc) const
	{
		Enc::Meter meter = enc.IncMeter(EncodeKey_Size());
		EncodeUInt32(enc, m_kind);
		if (HaveKey())
			EncodeField(enc, m_key, FldEncDisp::AsKey);
		EnsureThrow(meter.Met());
	}


	void Entity::EncodeEmptyKey(Enc& enc, uint32 kind)
	{
		Enc::Meter meter = enc.IncMeter(4);
		EncodeUInt32(enc, kind);
		EnsureThrow(meter.Met());
	}



	// Decode

	void Entity::DecodeOptDynE(Seq& s, Rp<Entity>& o)
	{
		// Assumes optional field is present. Must not be called if not present
		int64 kind;
		EnsureThrow(DecodeVarSInt64(s, kind));
		EnsureThrow(kind > 0);
		EnsureThrow(kind < UINT32_MAX);
		EntityCreator creator = GetEntityCreator((uint32) kind);
		EnsureThrow(creator != nullptr);
		o = creator(0, ObjId::None);

		uint64 nrFieldsToDecode;
		EnsureThrow(DecodeVarUInt64(s, nrFieldsToDecode));
		o->DecodeFieldsE(s, NumCast<sizet>(nrFieldsToDecode));
	}


	void Entity::DecodeVecDynE(Seq& s, RpVec<Entity>& v)
	{
		v.Clear();

		uint64 n = 0;
		EnsureThrow(DecodeVarUInt64(s, n));
		v.ReserveExact(NumCast<sizet>(n));

		for (sizet i=0; i!=n; ++i)
		{
			v.Add();

			int64 kind;
			EnsureThrow(DecodeVarSInt64(s, kind));

			if (kind > 0)
			{
				EnsureThrow(kind < UINT32_MAX);
				EntityCreator creator = GetEntityCreator((uint32) kind);
				EnsureThrow(creator != nullptr);
				v.Last().Set(creator(0, ObjId::None));

				uint64 nrFieldsToDecode;
				EnsureThrow(DecodeVarUInt64(s, nrFieldsToDecode));
				v.Last()->DecodeFieldsE(s, NumCast<sizet>(nrFieldsToDecode));
			}
		}
	}


	void Entity::DecodeFieldE(Seq& s, EntityFieldInfo const* efi, FldEncDisp::E fed)
	{
		if (fed == FldEncDisp::AsKey)
			EnsureThrow(KeyCat::IsKey(efi->m_keyCat));

		switch (efi->m_fieldType)
		{
		case FieldType::Entity:		DecodeValE(s, FieldRef<Entity>(*this, efi->m_offset), fed); break;
		case FieldType::Bool:		DecodeValE(s, FieldRef<bool  >(*this, efi->m_offset), fed); break;
		case FieldType::Enum:		DecodeValE(s, FieldRef<uint32>(*this, efi->m_offset), fed); break;
		case FieldType::UInt:		DecodeValE(s, FieldRef<uint64>(*this, efi->m_offset), fed); break;
		case FieldType::SInt:		DecodeValE(s, FieldRef<int64 >(*this, efi->m_offset), fed); break;
		case FieldType::Float:		DecodeValE(s, FieldRef<double>(*this, efi->m_offset), fed); break;
		case FieldType::Time:		DecodeValE(s, FieldRef<Time  >(*this, efi->m_offset), fed); break;
		case FieldType::ObjId:		DecodeValE(s, FieldRef<ObjId >(*this, efi->m_offset), fed); break;
		case FieldType::Str:		DecodeValE(s, FieldRef<Str   >(*this, efi->m_offset), fed); break;

		case FieldType::OptDyn:		DecodeOptDynE(s, FieldRef<Rp<Entity   >>(*this, efi->m_offset)); break;
		case FieldType::OptEnt:		DecodeOptE   (s, FieldRef<EntOptBase   >(*this, efi->m_offset)); break;
		case FieldType::OptBool:	DecodeOptE   (s, FieldRef<Opt<bool    >>(*this, efi->m_offset)); break;
		case FieldType::OptUInt:	DecodeOptE   (s, FieldRef<Opt<uint64  >>(*this, efi->m_offset)); break;
		case FieldType::OptSInt:	DecodeOptE   (s, FieldRef<Opt<int64   >>(*this, efi->m_offset)); break;
		case FieldType::OptFloat:	DecodeOptE   (s, FieldRef<Opt<double  >>(*this, efi->m_offset)); break;
		case FieldType::OptTime:	DecodeOptE   (s, FieldRef<Opt<Time    >>(*this, efi->m_offset)); break;
		case FieldType::OptObjId:	DecodeOptE   (s, FieldRef<Opt<ObjId   >>(*this, efi->m_offset)); break;
		case FieldType::OptStr:		DecodeOptE   (s, FieldRef<Opt<Str     >>(*this, efi->m_offset)); break;

		case FieldType::VecDyn:		DecodeVecDynE(s, FieldRef<RpVec<Entity>>(*this, efi->m_offset)); break;
		case FieldType::VecEnt:		DecodeVecE   (s, FieldRef<EntVecBase   >(*this, efi->m_offset)); break;
		case FieldType::VecBool:	DecodeVecE   (s, FieldRef<Vec<bool    >>(*this, efi->m_offset)); break;
		case FieldType::VecUInt:	DecodeVecE   (s, FieldRef<Vec<uint64  >>(*this, efi->m_offset)); break;
		case FieldType::VecSInt:	DecodeVecE   (s, FieldRef<Vec<int64   >>(*this, efi->m_offset)); break;
		case FieldType::VecFloat:	DecodeVecE   (s, FieldRef<Vec<double  >>(*this, efi->m_offset)); break;
		case FieldType::VecTime:	DecodeVecE   (s, FieldRef<Vec<Time    >>(*this, efi->m_offset)); break;
		case FieldType::VecObjId:	DecodeVecE   (s, FieldRef<Vec<ObjId   >>(*this, efi->m_offset)); break;
		case FieldType::VecStr:		DecodeVecE   (s, FieldRef<Vec<Str     >>(*this, efi->m_offset)); break;

		default:
			EnsureThrow(!"Unrecognized field type");
		}
	}


	void Entity::DecodeFieldsE(Seq& s, sizet nrFieldsToDecode)
	{
		ReInitFields();

		if (!s.n)              return;
		if (!nrFieldsToDecode) return;

		uint nrOptFieldsToGo {}, optFieldPresence {}, currentlyDecodingVersion {};
		while (true)
		{
			// We allow entity types to add new fields in the middle by encoding higher version fields after previous version fields.

			uint lowestNextVersion = UINT_MAX;
			for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
			{
				if (efi->m_version > currentlyDecodingVersion)
				{
					if (lowestNextVersion > efi->m_version)
						lowestNextVersion = efi->m_version;
				}
				else if (efi->m_version == currentlyDecodingVersion)
				{
					// The presence of optional fields is encoded with a single byte before each 0th, 8th, 16th, etc optional field.
					// The byte encodes the presence of up to 8 optional fields that follow, starting with least significant bit for first field.
					// The up to 8 optional fields whose presence is encoded by the byte can be interleaved with non-optional fields.

					if (!FieldType::IsOptional(efi->m_fieldType))
						DecodeFieldE(s, efi, FldEncDisp::AsField);
					else
					{
						if (nrOptFieldsToGo)
							--nrOptFieldsToGo;
						else
						{
							EnsureThrow(DecodeByte(s, optFieldPresence));
							nrOptFieldsToGo = 7;
						}
				
						if (((optFieldPresence << nrOptFieldsToGo) & 0x80) != 0)
							DecodeFieldE(s, efi, FldEncDisp::AsField);
					}

					if (!s.n)                return;
					if (!--nrFieldsToDecode) return;
				}
			}

			if (lowestNextVersion == UINT_MAX)
				return;

			currentlyDecodingVersion = lowestNextVersion;
		}
	}
}
