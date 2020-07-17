#include "AtIncludes.h"
#include "AtEntityStore.h"

#include "AtJsonGrammar.h"
#include "AtJsonReadWrite.h"
#include "AtUnicode.h"
#include "AtUtf8.h"

namespace At
{
	// Entity - Encode

	void Entity::JsonEncodeKind(Enc& enc, Entity const& entity)
	{
		enc.Ch('"').Fun(Json::EncodeStringPart, entity.GetKindName()).Ch('"');
	}


	void Entity::JsonEncodeEnumValue(Enc& enc, uint32 v, EntityFieldInfo const& efi)
	{
		enc.Ch('[').UInt(v).Add(",\"")
			.Fun(Json::EncodeStringPart, efi.m_enumTypeName).Ch('.').Fun(Json::EncodeStringPart, efi.m_enumToStrFunc(v)).Add("\"]");
	}


	void Entity::JsonEncodeStrValue(Enc& enc, Seq field)
	{
		if (!field.n)
			enc.Add("\"\"");
		else
		{
			// Does the field contain any non-space control characters?
			bool haveControlChars { Seq(field).DropToFirstByteOfType(Ascii::IsControlNonSpace).n != 0 };

			if (!haveControlChars && Utf8::IsValid(field))
			{
				// Encode field as a UTF-8 string
				enc.ReserveInc(4 + ((5 * field.n) / 4));

				enc.Add("\"s:");
				Json::EncodeStringPart(enc, field);
				enc.Ch('"');
			}
			else
			{
				// Encode field as a hex string
				enc.ReserveInc(4 + (2 * field.n));

				enc.Add("\"h:");
				while (field.n)
					enc.Hex(field.ReadByte());
				enc.Add("\"");
			}
		}
	}


	void Entity::JsonEncodeDynEntity(Enc& enc, Entity const& e)
	{
		enc.Add("{\"k\":").Fun(JsonEncodeKind, e).Add(",\"v\":");
		e.JsonEncode(enc);
		enc.Add("}");
	}


	void Entity::JsonEncodeDynEntityOrNull(Enc& enc, Rp<Entity> const& e)
	{
		if (!e.Any())
			enc.Add("null");
		else
			JsonEncodeDynEntity(enc, e.Ref());
	}


	void Entity::JsonEncodeFieldValue(Enc& a, EntityFieldInfo const& efi) const
	{
		switch (efi.m_fieldType)
		{
		case FieldType::Entity:   FieldRef<Entity>(*this, efi.m_offset).JsonEncode(a);									break;
		case FieldType::Bool:     a.Add(If(FieldRef<bool>(*this, efi.m_offset), Seq, "true", "false"));					break;
		case FieldType::Enum:     JsonEncodeEnumValue(a, FieldRef<uint32>(*this, efi.m_offset), efi);					break;
		case FieldType::UInt:     a.UInt(FieldRef<uint64>(*this, efi.m_offset));										break;
		case FieldType::SInt:     a.SInt(FieldRef<int64>(*this, efi.m_offset));											break;
		case FieldType::Float:    a.Dbl(FieldRef<double>(*this, efi.m_offset), Entity_JsonFloatPrec);					break;
		case FieldType::Time:     Json::EncodeTime(a, FieldRef<Time>(*this, efi.m_offset));                             break;
		case FieldType::ObjId:    a.Ch('"').Obj(FieldRef<ObjId>(*this, efi.m_offset)).Ch('"');							break;
		case FieldType::Str:      JsonEncodeStrValue(a, FieldRef<Str>(*this, efi.m_offset));							break;

		case FieldType::OptDyn:   { Rp<Entity>    const& o(FieldRef<Rp<Entity   >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else JsonEncodeDynEntity(a, o.Ref());                         break; }
		case FieldType::OptEnt:   { EntOptBase    const& o(FieldRef<EntOptBase   >(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else o.Ref().JsonEncode(a);                                   break; } 
		case FieldType::OptBool:  { Opt<bool>     const& o(FieldRef<Opt<bool    >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else a.Add(If(o.Ref(), Seq, "true", "false"));                break; } 
		case FieldType::OptUInt:  { Opt<uint64>   const& o(FieldRef<Opt<uint64  >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else a.UInt(o.Ref());                                         break; } 
		case FieldType::OptSInt:  { Opt<int64>    const& o(FieldRef<Opt<int64   >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else a.SInt(o.Ref());                                         break; } 
		case FieldType::OptFloat: { Opt<double>   const& o(FieldRef<Opt<double  >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else a.Dbl(o.Ref());                                          break; } 
		case FieldType::OptTime:  { Opt<Time>     const& o(FieldRef<Opt<Time    >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else Json::EncodeTime(a, o.Ref());                            break; } 
		case FieldType::OptObjId: { Opt<ObjId>    const& o(FieldRef<Opt<ObjId   >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else a.Ch('"').Obj(o.Ref()).Ch('"');                          break; } 
		case FieldType::OptStr:   { Opt<Str>      const& o(FieldRef<Opt<Str     >>(*this, efi.m_offset)); if (!o.Any()) a.Add("null"); else JsonEncodeStrValue(a, o.Ref());                          break; } 

		case FieldType::VecDyn:   { RpVec<Entity> const& v(FieldRef<RpVec<Entity>>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { JsonEncodeDynEntityOrNull(a, v[i]);    } ); break; }
		case FieldType::VecEnt:   { EntVecBase    const& v(FieldRef<EntVecBase   >(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { v[i].JsonEncode(a);                    } ); break; }
		case FieldType::VecBool:  { Vec<bool>     const& v(FieldRef<Vec<bool    >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { a.Add(If(v[i], Seq, "true", "false")); } ); break; }
		case FieldType::VecUInt:  { Vec<uint64>   const& v(FieldRef<Vec<uint64  >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { a.UInt(v[i]);                          } ); break; }
		case FieldType::VecSInt:  { Vec<int64>    const& v(FieldRef<Vec<int64   >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { a.SInt(v[i]);                          } ); break; }
		case FieldType::VecFloat: { Vec<double>   const& v(FieldRef<Vec<double  >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { a.Dbl(v[i], Entity_JsonFloatPrec);     } ); break; }
		case FieldType::VecTime:  { Vec<Time>     const& v(FieldRef<Vec<Time    >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { Json::EncodeTime(a, v[i]);             } ); break; }
		case FieldType::VecObjId: { Vec<ObjId>    const& v(FieldRef<Vec<ObjId   >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { a.Ch('"').Obj(v[i]).Ch('"');           } ); break; }
		case FieldType::VecStr:   { Vec<Str>      const& v(FieldRef<Vec<Str     >>(*this, efi.m_offset)); Json::EncodeArray(a, v.Len(), [&] (Enc& a, sizet i) { JsonEncodeStrValue(a, v[i]);           } ); break; }

		default:
			EnsureThrow(!"Unrecognized field type");
		}
	}


	void Entity::JsonEncode(Enc& enc) const
	{
		enc.Ch('{');

		bool needComma {};
		for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
		{
			if (needComma)
				enc.Ch(',');
			else
				needComma = true;

			Json::EncodeString(enc, efi->m_fieldName);
			enc.Ch(':');
			JsonEncodeFieldValue(enc, *efi);
		}

		enc.Ch('}');
	}



	// Entity - Decode

	ObjId Entity::JsonDecodeObjId(ParseNode const& p, JsonIds* jsonIds)
	{
		if (!p.IsType(Json::id_String))
			if (jsonIds)
				throw Json::DecodeErr(p, "Expected string containing JSON ID");
			else
				throw Json::DecodeErr(p, "Expected string containing ObjId");

		Str s;
		Json::DecodeString(p, s);

		if (jsonIds)
		{
			JsonIds::ConstIt it = jsonIds->Find(s);
			if (!it.Any())
				throw Json::DecodeErr(p, "Could not find JSON ID");

			return it->m_objId;
		}
		else
		{
			Seq reader { s };
			ObjId objId;
			if (!objId.ReadStr(reader) || reader.n != 0)
				throw Json::DecodeErr(p, "Expected ObjId");

			return objId;
		}
	}


	uint32 Entity::JsonDecodeUInt32WDesc(ParseNode const& p)
	{
		uint64 v {};
		if (p.IsType(Json::id_Number))
			v = Json::DecodeUInt(p);
		else
		{
			int index {};
			Json::DecodeArray(p, [&] (ParseNode const& c)
				{
					if (index == 0)
						v = Json::DecodeUInt(c);
					else if (index == 1)
					{
						if (!c.IsType(Json::id_String))           throw Json::DecodeErr(c, "Expected string description of uint32 value");
						if (c.SrcText().n > MaxJsonUInt32DescLen) throw Json::DecodeErr(c, "String description of uint32 value is too long");
					}
					else
						throw Json::DecodeErr(c, "Expected end of uint32-with-description array");

					++index;
				} );
		}

		if (v > UINT32_MAX)
			throw Json::DecodeErr(p, "Numeric value is too large");

		return (uint32) v;
	}
	

	template <class StrType>
	void Entity::JsonDecodeStrValue(ParseNode const& p, StrType& field)
	{
		if (!p.IsType(Json::id_String))
			throw Json::DecodeErr(p, "Expected string");

		Str s;
		Json::DecodeString(p, s);

		Seq reader { s };
		if (reader.StartsWithExact("h:"))
		{
			reader.DropBytes(2);
			
			uint c;
			sizet nrBytes { reader.n / 2 };
			field.Clear();
			while ((c = reader.ReadHexEncodedByte()) != UINT_MAX)
				field.Byte((byte) c);

			if (reader.n != 0)
				throw Json::DecodeErr(p, "Hex string contains non-hex characters");

			if (field.Len() != nrBytes)
				throw Json::DecodeErr(p, "Hex string too long to store in this field");
		}
		else
		{
			if (reader.StartsWithExact("s:"))
				reader.DropBytes(2);

			field.Set(reader);

			if (field.Len() != reader.n)
				throw Json::DecodeErr(p, "String too long to store in this field");
		}
	}


	void Entity::JsonDecodeDynEntity(ParseNode const& p, Rp<Entity>& o, EntityStore* storePtr, JsonIds* jsonIds)
	{
		if (!p.IsType(Json::id_Object))
			throw Json::DecodeErr(p, "Expected object");

		enum DynEntityState { ExpectParent, ExpectJsonId, ExpectKind, ExpectValue, ExpectEnd };
		DynEntityState state { ExpectParent };
		ObjId parentId;

		if (!jsonIds)
			state = ExpectKind;

		Json::DecodeObject(p, [&] (ParseNode const& nameParser, Seq name, ParseNode const& valueParser) -> bool
			{
				if (state == ExpectParent)
				{
					if (!name.EqualExact("p"))
						throw Json::DecodeErr(nameParser, "Expected entity parent 'p'");

					Str parentJsonId;
					Json::DecodeString(valueParser, parentJsonId);
					JsonIds::ConstIt it = jsonIds->Find(parentJsonId);
					if (!it.Any())
						throw Json::DecodeErr(nameParser, "Could not find entity parent JSON ID");

					parentId = it->m_objId;
					state = ExpectKind;
				}
				else if (state == ExpectKind)
				{
					if (!name.EqualExact("k"))
						throw Json::DecodeErr(nameParser, "Expected entity kind 'k'");
					
					Str kindName;
					Json::DecodeString(valueParser, kindName);
					uint32 kind = Crc32(kindName);

					EntityCreator creator { GetEntityCreator(kind) };
					if (!creator)
						throw Json::DecodeErr(valueParser, "Unrecognized entity kind");

					o.Set(creator(storePtr, parentId));
					state = ExpectValue;
				}
				else if (state == ExpectValue)
				{
					if (!name.EqualExact("v"))
						throw Json::DecodeErr(nameParser, "Expected entity value 'v'");

					o->JsonDecode(valueParser, jsonIds);
					state = ExpectEnd;
				}
				else
					throw Json::DecodeErr(nameParser, "Expected end of JSON object");

				return true;
			} );

		if (state != ExpectEnd)
			throw Json::DecodeErr(p, "Incomplete JSON entity encoding");
	}


	void Entity::JsonDecodeDynEntityOrNull(ParseNode const& p, Rp<Entity>& o)
	{
		if (p.IsType(Json::id_Null))
			o.Clear();
		else
			JsonDecodeDynEntity(p, o, nullptr, nullptr);
	}


	void Entity::JsonDecodeFieldValue(ParseNode const& p, EntityFieldInfo const& efi, JsonIds* jsonIds)
	{
		switch (efi.m_fieldType)
		{
		case FieldType::Entity:   FieldRef<Entity>(*this, efi.m_offset).JsonDecode(p, jsonIds);        break;
		case FieldType::Bool:     FieldRef<bool  >(*this, efi.m_offset) = Json::DecodeBool      (p);   break;
		case FieldType::Enum:     FieldRef<uint32>(*this, efi.m_offset) = JsonDecodeUInt32WDesc (p);   break;
		case FieldType::UInt:     FieldRef<uint64>(*this, efi.m_offset) = Json::DecodeUInt      (p);   break;
		case FieldType::SInt:     FieldRef<int64 >(*this, efi.m_offset) = Json::DecodeSInt      (p);   break;
		case FieldType::Float:    FieldRef<double>(*this, efi.m_offset) = Json::DecodeFloat     (p);   break;
		case FieldType::Time:     FieldRef<Time  >(*this, efi.m_offset) = Json::DecodeTime      (p);   break;
		case FieldType::ObjId:    FieldRef<ObjId >(*this, efi.m_offset) = JsonDecodeObjId(p, jsonIds); break;
		case FieldType::Str:      JsonDecodeStrValue(p, FieldRef<Str>(*this, efi.m_offset));           break;

		case FieldType::OptDyn:   { Rp<Entity>&    o(FieldRef<Rp<Entity   >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else JsonDecodeDynEntity(p, o, nullptr, nullptr); break; }
		case FieldType::OptEnt:   { EntOptBase&    o(FieldRef<EntOptBase   >(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init().JsonDecode(p, jsonIds);             break; }
		case FieldType::OptBool:  { Opt<bool  >&   o(FieldRef<Opt<bool    >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = Json::DecodeBool  (p);            break; }
		case FieldType::OptUInt:  { Opt<uint64>&   o(FieldRef<Opt<uint64  >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = Json::DecodeUInt  (p);            break; }
		case FieldType::OptSInt:  { Opt<int64 >&   o(FieldRef<Opt<int64   >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = Json::DecodeSInt  (p);            break; }
		case FieldType::OptFloat: { Opt<double>&   o(FieldRef<Opt<double  >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = Json::DecodeFloat (p);            break; }
		case FieldType::OptTime:  { Opt<Time  >&   o(FieldRef<Opt<Time    >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = Json::DecodeTime  (p);            break; }
		case FieldType::OptObjId: { Opt<ObjId >&   o(FieldRef<Opt<ObjId   >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else o.Init() = JsonDecodeObjId(p, jsonIds);      break; }
		case FieldType::OptStr:   { Opt<Str   >&   o(FieldRef<Opt<Str     >>(*this, efi.m_offset)); if (p.IsType(Json::id_Null)) o.Clear(); else JsonDecodeStrValue(p, o.Init());             break; }

		case FieldType::VecDyn:   { RpVec<Entity>& v(FieldRef<RpVec<Entity>>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { JsonDecodeDynEntityOrNull(p, v.Add()); } ); break; }
		case FieldType::VecEnt:   { EntVecBase&    v(FieldRef<EntVecBase   >(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add().JsonDecode(p, jsonIds);        } ); break; }
		case FieldType::VecBool:  { Vec<bool  >&   v(FieldRef<Vec<bool    >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = Json::DecodeBool  (p);       } ); break; }
		case FieldType::VecUInt:  { Vec<uint64>&   v(FieldRef<Vec<uint64  >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = Json::DecodeUInt  (p);       } ); break; }
		case FieldType::VecSInt:  { Vec<int64 >&   v(FieldRef<Vec<int64   >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = Json::DecodeSInt  (p);       } ); break; }
		case FieldType::VecFloat: { Vec<double>&   v(FieldRef<Vec<double  >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = Json::DecodeFloat (p);       } ); break; }
		case FieldType::VecTime:  { Vec<Time  >&   v(FieldRef<Vec<Time    >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = Json::DecodeTime  (p);       } ); break; }
		case FieldType::VecObjId: { Vec<ObjId >&   v(FieldRef<Vec<ObjId   >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { v.Add() = JsonDecodeObjId(p, jsonIds); } ); break; }
		case FieldType::VecStr:   { Vec<Str   >&   v(FieldRef<Vec<Str     >>(*this, efi.m_offset)); v.Clear(); Json::DecodeArray(p, [&] (ParseNode const& p) { JsonDecodeStrValue(p, v.Add());        } ); break; }

		default:
			EnsureThrow(!"Unrecognized field type");
		}
	}


	void Entity::JsonDecode(ParseNode const& p, JsonIds* jsonIds)
	{
		Json::DecodeObject(p, [&] (ParseNode const& nameParser, Seq name, ParseNode const& valueParser) -> bool
			{
				bool fieldFound = false;
				for (EntityFieldInfo const* efi=m_fields; !efi->IsPastEnd(); ++efi)
					if (name.EqualExact(efi->m_fieldName))
					{
						JsonDecodeFieldValue(valueParser, *efi, jsonIds);
						fieldFound = true;
						break;
					}

				if (!fieldFound)
					throw Json::DecodeErr(nameParser, Str("No field with this name found in entity of type ").Add(m_kindName));

				return true;
			} );
	}



	// EntityChildInfo

	Str EntityChildInfo::JsonEncodeKey() const
	{
		switch (m_keyFieldType)
		{
		case FieldType::PastEnd:	return "null";
		case FieldType::UInt:		return Str().UInt(u_keyUInt);
		case FieldType::SInt:		return Str().SInt(u_keySInt);
		case FieldType::Float:		return Str().Dbl(u_keyFloat, Entity_JsonFloatPrec);
		case FieldType::Time:		return Str().Ch('"').Obj(m_keyTime, TimeFmt::IsoMicroZ, 'T').Ch('"');
		case FieldType::ObjId:		return Str().Ch('"').Obj(m_keyObjId).Ch('"');
		case FieldType::Str:		{ Str s; Entity::JsonEncodeStrValue(s, m_keyStr); return s; }
		default:					return "<unknown key field type>";
		}
	}
}
