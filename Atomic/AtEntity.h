#pragma once

#include "AtCrc32.h"
#include "AtDescEnum.h"
#include "AtEncode.h"
#include "AtEntityKinds.h"
#include "AtJsonReadWrite.h"
#include "AtMap.h"
#include "AtNumCvt.h"
#include "AtOpt.h"
#include "AtParse.h"
#include "AtRp.h"
#include "AtTreeStore.h"


namespace At
{

	// Entity: A persistable data object base type for use with EntityStore.
	//
	// Individual types of entity objects are defined using macros defined here.
	// Each entity object type is a "kind". Each entity kind has an ID, which is obtained as CRC32 of the name of the entity type.
	// Every entity instance can be encoded in binary (for storage using EntityStore) and in JSON (for data interchange and display to user).
	//
	// The binary and JSON encodings have the following properties:
	//
	// - Any entity can be encoded and decoded by the same version of the application, and will retain the information it contains.
	//
	// - Future application versions can ensure compatibility with data encoded by past versions by following these rules:
	//
	//   - Not renaming the entity kind. There is currently a 1-to-1 correspondence between kind name and ID: the ID is a CRC32 of the name.
	//     If support for entity kind renaming is implemented, take into account that for the binary format, preserving backward compatibility
	//     means recognizing the previous kind ID; but for JSON, preserving backward compatibility means recognizing the previous kind name.
	//
	//   - If an entity field is renamed, this will preserve compatibility with past binary encodings (where field names are not mentioned),
	//     but will break JSON (where field names are referenced in encoding).
	//
	//   - If the order of fields is changed, this will break the binary encoding (which relies on order), but not JSON (uses field names).
	//
	//   - If a field is ever removed, or if an existing field type is changed, this will break both binary and JSON encodings.
	//
	// New versions of applications should:
	//
	// - Be careful to preserve kind IDs. Currently, unless support for renaming is added, this means not renaming entities.
	//
	// - Avoid creating kind ID conflicts when adding new entities.
	//
	// - Avoid renaming fields if JSON backward compatibility is a goal. It's OK to rename fields with respect to the binary format.
	//
	// - Avoid reordering fields if binary backward compatibility is a goal. It's OK to reorder fields with respect to the JSON format.
	//
	// - Avoid removing fields, or changing their types, if any type of backward compatibility is desired.
	//
	// Fields can be added freely:
	//
	// - At the END of existing entity kinds, without having to give them an incremented version.
	// 
	// - BEFORE the end of existing entity kinds, if they are given an incremented version.
	//
	// When decoding entities stored by a previous version that did not encode the newer fields, their values will be left at defaults.
	// Older application versions, however, will NOT be able to read entities that encode new fields.


	struct FieldType
	{
		enum
		{
			PastEnd	    = 0x00,
		
			// Singular, not indexable
			Entity      = 0x01,
			Bool        = 0x02,
			Enum        = 0x03,

			// Singular, non-optional, indexable
			UInt        = 0x10,
			SInt        = 0x11,
			Float       = 0x12,
			Time        = 0x13,
			ObjId       = 0x14,
			Str	        = 0x15,
		
			// Singular, optional, not indexable
			OptDyn      = 0x20,
			OptEnt      = 0x21,
			OptBool     = 0x22,
			OptUInt     = 0x23,
			OptSInt     = 0x24,
			OptFloat    = 0x25,
			OptTime     = 0x26,
			OptObjId    = 0x27,
			OptStr      = 0x28,
		
			// Vector, not indexable
			VecDyn      = 0x30,
			VecEnt      = 0x31,
			VecBool     = 0x32,
			VecUInt     = 0x33,
			VecSInt     = 0x34,
			VecFloat    = 0x35,
			VecTime     = 0x36,
			VecObjId    = 0x37,
			VecStr      = 0x38,
		};
	
		static bool IsIndexable (uint ft) { return (ft & 0xF0) == 0x10; }
		static bool IsOptional  (uint ft) { return (ft & 0xF0) == 0x20; }

		static char const* GetName(uint ft);
	};



	struct KeyCat
	{
		enum E
		{
			KeyBit                      = 1,
			StrBit                      = 2,
			UniqueBit                   = 4,
			SensitiveBit                = 8,

			None						= (0      | 0      | 0         | 0           ),

			Key_NonStr_Multi			= (KeyBit | 0      | 0         | 0           ),
			Key_NonStr_Unique			= (KeyBit | 0      | UniqueBit | 0           ),

			Key_Str_Multi_Insensitive	= (KeyBit | StrBit | 0         | 0           ),
			Key_Str_Multi_Sensitive		= (KeyBit | StrBit | 0         | SensitiveBit),
			Key_Str_Unique_Insensitive	= (KeyBit | StrBit | UniqueBit | 0           ),
			Key_Str_Unique_Sensitive    = (KeyBit | StrBit | UniqueBit | SensitiveBit),
		};
	
		static bool IsKey            (uint role) { return (role & (KeyBit               )) == (KeyBit               ); }

		static bool IsNonStrKey      (uint role) { return (role & (KeyBit | StrBit      )) == (KeyBit | 0           ); }
		static bool IsStrKey         (uint role) { return (role & (KeyBit | StrBit      )) == (KeyBit | StrBit      ); }

		static bool IsMultiKey       (uint role) { return (role & (KeyBit | UniqueBit   )) == (KeyBit | 0           ); }
		static bool IsUniqueKey      (uint role) { return (role & (KeyBit | UniqueBit   )) == (KeyBit | UniqueBit   ); }

		static bool IsInsensitiveKey (uint role) { return (role & (KeyBit | SensitiveBit)) == (KeyBit | 0           ); }
		static bool IsSensitiveKey   (uint role) { return (role & (KeyBit | SensitiveBit)) == (KeyBit | SensitiveBit); }
	};



	class EntOptBase
	{
	public:
		// These methods need to match the ones defined by Opt,
		// so they can be used in templates that also take Opt
		Entity&       Init()               { return EOB_Init(); }
		void          Clear() noexcept     {        EOB_Clear(); }
		bool          Any() const noexcept { return EOB_Any(); }
		Entity&       Ref()                { return EOB_Ref(); }
		Entity const& Ref() const          { return EOB_Ref(); }

	protected:
		// These methods need to NOT match the methods defined by Opt,
		// so they won't conflict with methods EntOpt<T> derives from Opt<T>.
		virtual Entity&       EOB_Init() = 0;
		virtual void          EOB_Clear() noexcept = 0;
		virtual bool          EOB_Any() const noexcept = 0;
		virtual Entity&       EOB_Ref() = 0;
		virtual Entity const& EOB_Ref() const = 0;
	};



	class EntVecBase
	{
	public:
		// These methods need to match the ones defined by Vec,
		// so they can be used in templates that also take Vec
		EntVecBase&   ReserveExact(sizet i)      { EVB_ReserveExact(i); return *this; }
		sizet         Len() const noexcept       { return EVB_Len(); }
		Entity&       operator[] (sizet i)       { return EVB_At(i); }
		Entity const& operator[] (sizet i) const { return EVB_At(i); }
		EntVecBase&   Clear() noexcept           { EVB_Clear(); return *this; }
		Entity&       Add()                      { return EVB_Add(); }

	protected:
		// These methods need to NOT match the methods defined by Vec,
		// so they won't conflict with methods EntVec<T> derives from Vec<T>.
		virtual void          EVB_ReserveExact(sizet i) = 0;
		virtual sizet         EVB_Len() const noexcept = 0;
		virtual Entity&       EVB_At(sizet i) = 0;
		virtual Entity const& EVB_At(sizet i) const = 0;
		virtual void          EVB_Clear() noexcept = 0;
		virtual Entity&       EVB_Add() = 0;
	};



	struct EntityFieldInfo
	{
		EntityFieldInfo() = default;

		EntityFieldInfo(char const* fieldName, sizet offset, uint fieldType, KeyCat::E keyCat, uint version)
			: m_fieldName(fieldName), m_offset(offset), m_fieldType(fieldType), m_keyCat(keyCat), m_version(version)
			{ EnsureThrow(fieldType != FieldType::Enum); }

		EntityFieldInfo(char const* fieldName, sizet offset, char const* enumTypeName, DescEnumToStrFunc enumToStrFunc, uint version)
			: m_fieldName(fieldName), m_offset(offset), m_enumTypeName(enumTypeName), m_enumToStrFunc(enumToStrFunc), m_fieldType(FieldType::Enum), m_version(version) {}

		bool IsPastEnd() const { return m_fieldType == FieldType::PastEnd; }

		char const*       const m_fieldName     {};
		sizet             const m_offset        {};
		Seq               const m_enumTypeName  {};
		DescEnumToStrFunc const m_enumToStrFunc {};
		uint              const m_fieldType     { FieldType::PastEnd };
		KeyCat::E         const m_keyCat        { KeyCat::None };
		uint              const m_version       {};		// First version of entity in which this field appears
	};



	struct EntityChildInfo;

	enum { Entity_JsonFloatPrec = 8 };		// If defined within Entity class, causes compilation errors when used with lambdas in VS 2010

	class Entity : public RefCountable
	{
	public:
		ObjId m_parentId;
		ObjId m_entityId;		// Use as refObjId when looking up this entity's parent using m_parentId
		ObjId m_contentObjId;	// Use as refObjId when looking up any entity using an ObjId contained in the fields of this entity

		enum EContainedOrTbd { Contained, ParentToBeDetermined };
		enum EChildOf { ChildOf };

		Entity(EntityStore* store, uint32 kind, char const* kindName, uint32 nrFields, EntityFieldInfo const* fields, EntityFieldInfo const* key, ObjId parentId);
	
		virtual ~Entity() {}
	
		EntityStore&           GetStore       () const { EnsureThrow(m_store != nullptr); return *m_store; }
		uint32                 GetKind        () const { return m_kind; }
		char const*            GetKindName    () const { return m_kindName; }
		uint32                 GetNrFields    () const { return m_nrFields; }
		EntityFieldInfo const* GetFields      () const { return m_fields; }
		EntityFieldInfo const* GetFieldByName (Seq name) const;
		bool                   HaveKey        () const { return !m_key->IsPastEnd(); } 
		EntityFieldInfo const* GetKey         () const { return m_key; }
		bool                   UniqueKey      () const { return KeyCat::IsUniqueKey(m_key->m_keyCat); }	

		void JsonEncodeFieldValue(Enc& enc, EntityFieldInfo const& efi) const;
		void JsonEncode(Enc& enc) const;

		struct JsonId
		{
			Str   m_jsonId;
			ObjId m_objId;

			Str const& Key() const { return m_jsonId; }
		};

		using JsonIds = Map<JsonId>;

		void JsonDecodeFieldValue(ParseNode const& p, EntityFieldInfo const& efi, JsonIds* jsonIds);
		void JsonDecode(ParseNode const& p, JsonIds* jsonIds);

		// Used to encode both dynamic-type entities embedded as part of other entities (jsonIds == nullptr),
		// and dynamic-type entities that are part of an import script (jsonIds != nullptr).
		// For embedded entities, expects a JSON object with members "k" (kind, string) followed by "v" (entity value, JSON object).
		// For import script entities, expects additional member "p" (parent, string) before "k". Parent must be found in jsonIds.
		static void JsonDecodeDynEntity(ParseNode const& p, Rp<Entity>& o, EntityStore* storePtr, JsonIds* jsonIds);

		void ReInitFields();

		// Can only be used if the entity was loaded through Load() or FindChildren()
		bool HasChildren() const { EnsureThrow(m_hasChildrenState != HasChildrenState::Unknown); return m_hasChildrenState != HasChildrenState::No; }

		struct PutType { enum E { New, Existing }; };

		void       Load           (ObjId refObjId);
		void       Put            (PutType::E putType, ObjId parentRefObjId);
		ChildCount RemoveChildren ();
		void       Remove         ();

		// It is okay to pass ObjId::None for parentRefObjId, if and only if the parent has been previously read by the current transaction.
		// If the parent has not been loaded, EnsureAbort will be triggered if parent is removed, and there is no parentRefObjId to cause a RetryTxException.
		void Insert_ParentRef            (ObjId parentRefObjId) { Put(PutType::New, parentRefObjId); }
		void Insert_ParentLoaded         ()                     { Put(PutType::New, ObjId::None); }
		void Insert_ParentExists         ()                     { Put(PutType::New, ObjId::None); }
		void Update                      ()                     { Put(PutType::Existing, ObjId::None); }
		void UpdateOrInsert_ParentRef    (ObjId parentRefObjId) { Put(m_entityId.Any() ? PutType::Existing : PutType::New, parentRefObjId); }
		void UpdateOrInsert_ParentLoaded ()                     { Put(m_entityId.Any() ? PutType::Existing : PutType::New, ObjId::None); }

		template <class ParentType>
		Rp<ParentType> GetParent() const
			{ return m_store->GetEntityOfKind<ParentType>(m_parentId, m_entityId); }

		// Use this to load any entity which is referenced by an ObjId contained in this entity.
		// Do NOT use this to load this entity's parent: that would load the parent using the wrong refObjId,
		// leading to subtle errors and inconsistencies. To load this entity's parent, use GetParent().
		template <class EntityType>
		Rp<EntityType> GetReferencedEntity(ObjId entityId) const
			{ return m_store->GetEntityOfKind<EntityType>(entityId, m_contentObjId); }

		template <EnumDir Direction = EnumDir::Forward>
		void EnumAllChildren(std::function<bool (EntityChildInfo const&)> onMatch)
		{
			EnsureThrow(m_store != nullptr);
			m_store->EnumAllChildren<Direction>(m_entityId, onMatch);
		}

		template <EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(uint32 kind, std::function<bool (EntityChildInfo const&)> onMatch)
		{
			EnsureThrow(m_store != nullptr);
			m_store->EnumAllChildrenOfKind<Direction>(m_entityId, kind, onMatch);
		}

		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(std::function<bool (Rp<ChildType> const&)> onMatch)
		{
			EnsureThrow(m_store != nullptr);
			m_store->EnumAllChildrenOfKind<ChildType, Direction>(m_entityId, onMatch);
		}

		template <class ChildType, class ContainerType, EnumDir Direction = EnumDir::Forward>
		void EnumAllChildrenOfKind(ContainerType& container)
		{
			EnsureThrow(m_store != nullptr);
			m_store->EnumAllChildrenOfKind<ChildType, ContainerType, Direction>(m_entityId, container);
		}

		template <class ChildType>
		ChildCount RemoveAllChildrenOfKind()
		{
			EnsureThrow(m_store != nullptr);
			return m_store->RemoveAllChildrenOfKind<ChildType>(m_entityId);
		}

		template <class ChildType>
		Rp<ChildType> GetFirstChildOfKind()
		{
			EnsureThrow(m_store != nullptr);
			return m_store->GetFirstChildOfKind<ChildType>(m_entityId);
		}

		template <class ChildType, EnumDir Direction = EnumDir::Forward>
		void FindChildren(typename ChildType::KeyType const& keyFirst, typename ChildType::KeyType const* keyBeyondLast, std::function<bool (Rp<ChildType> const&)> onMatch)
		{
			EnsureThrow(m_store != nullptr);
			m_store->FindChildren<ChildType, Direction>(m_entityId, keyFirst, keyBeyondLast, onMatch);
		}

		template <class ChildType>
		Rp<ChildType> FindChild(typename ChildType::KeyType const& key)
		{
			EnsureThrow(m_store != nullptr);
			return m_store->FindChild<ChildType>(m_entityId, key);
		}

		template <class ChildType>
		ObjId FindChildId(typename ChildType::KeyType const& key)
		{
			EnsureThrow(m_store != nullptr);
			return m_store->FindChildId<ChildType>(m_entityId, key);
		}

		template <class ChildType>
		bool GetLastChildInfoOfKind(typename ChildType::KeyType& foundKey, ObjId& foundId, ObjId& foundBucketId)
		{
			EnsureThrow(m_store != nullptr);
			return m_store->GetLastChildInfoOfKind<ChildType>(m_entityId, foundKey, foundId, foundBucketId);
		}

		static uint64 NextKey(uint64 v)     { return SatAdd<uint64>(v, 1); }
		static int64  NextKey(int64 v)      { return SatAdd<int64>(v, 1); }
		static Time   NextKey(Time v)       { return ++v; }
		static Str    NextKey(Str const& v) { Str k(v); k.Byte(0); return k; }
		static ObjId  NextKey(ObjId v)		{ return ObjId::Next(v); } 

	protected:
		EntityStore*           m_store;
		uint32                 m_kind;
		char const*            m_kindName;
		uint32                 m_nrFields;
		EntityFieldInfo const* m_fields;
		EntityFieldInfo const* m_key;
	
		static EntityStore* GetStorePtr(Entity const& x) { return x.m_store; } 

		struct HasChildrenState { enum E { Unknown, Yes, No }; };
		HasChildrenState::E m_hasChildrenState;

	protected:
		enum { MaxJsonUInt32DescLen = 1 + DescEnum_MaxTypeNameLen + 1 + DescEnum_MaxDescLen + 1 };
		static void   JsonEncodeKind            (Enc& enc, Entity const& entity);
		static void   JsonEncodeEnumValue       (Enc& enc, uint32 v, EntityFieldInfo const& efi);
		static void   JsonEncodeStrValue        (Enc& enc, Seq field);
		static void   JsonEncodeDynEntityOrNull (Enc& enc, Rp<Entity> const& o);

		static ObjId  JsonDecodeObjId           (ParseNode const& p, JsonIds* jsonIds);
		static uint32 JsonDecodeUInt32WDesc     (ParseNode const& p);
		template <class StrType>
		static void   JsonDecodeStrValue        (ParseNode const& p, StrType& field);
		static void   JsonDecodeDynEntityOrNull (ParseNode const& p, Rp<Entity>& o);

	protected:
		struct FldEncDisp { enum E { AsField, AsKey }; };

		static sizet EncodeVal_Size (bool           , FldEncDisp::E     ) { return 1; }
		static sizet EncodeVal_Size (Entity const& v, FldEncDisp::E     ) { return EncodeVarUInt64_Size(v.GetNrFields()) + v.EncodeFields_Size(); }
		static sizet EncodeVal_Size (uint64        v, FldEncDisp::E     ) { return EncodeVarUInt64_Size(v); }
		static sizet EncodeVal_Size (int64         v, FldEncDisp::E     ) { return EncodeVarSInt64_Size(v); }
		static sizet EncodeVal_Size (double         , FldEncDisp::E fed ) { return (sizet) ((fed == FldEncDisp::AsKey) ? EncodeSortDouble_Size : EncodeDouble_Size); }
		static sizet EncodeVal_Size (Time          v, FldEncDisp::E     ) { return EncodeVarUInt64_Size(v.ToFt()); }
		static sizet EncodeVal_Size (ObjId         v, FldEncDisp::E     ) { return EncodeVarUInt64_Size(v.m_uniqueId) + EncodeVarUInt64_Size(v.m_index); }
		static sizet EncodeVal_Size (Str    const& v, FldEncDisp::E fed ) { return (fed == FldEncDisp::AsKey) ? EncodeSortStr_Size(v) : EncodeVarStr_Size(v.Len()); }

		static void  EncodeVal (Enc& enc, bool          v, FldEncDisp::E     ) { EncodeByte(enc, (byte) (v ? 148 : 48)); }	// VarUInt64 encodings for 100 and 0, respectively
		static void  EncodeVal (Enc& enc, Entity const& v, FldEncDisp::E     ) { EncodeVarUInt64(enc, v.GetNrFields()); v.EncodeFields(enc); }
		static void  EncodeVal (Enc& enc, uint64        v, FldEncDisp::E     ) { EncodeVarUInt64(enc, v); }
		static void  EncodeVal (Enc& enc, int64         v, FldEncDisp::E     ) { EncodeVarSInt64(enc, v); }
		static void  EncodeVal (Enc& enc, double        v, FldEncDisp::E fed ) { if (fed == FldEncDisp::AsKey) EncodeSortDouble(enc, v); else EncodeDouble(enc, v); }
		static void  EncodeVal (Enc& enc, Time          v, FldEncDisp::E     ) { EncodeVarUInt64(enc, v.ToFt()); }
		static void  EncodeVal (Enc& enc, ObjId         v, FldEncDisp::E     ) { EncodeVarUInt64(enc, v.m_uniqueId); EncodeVarUInt64(enc, v.m_index); }
		static void  EncodeVal (Enc& enc, Str    const& v, FldEncDisp::E fed ) { if (fed == FldEncDisp::AsKey) EncodeSortStr(enc, v); else EncodeVarStr(enc, v); }

		static sizet EncodeOptDyn_Size (Rp<Entity> const& v);
		static void  EncodeOptDyn      (Enc& enc, Rp<Entity> const& v);

		static sizet EncodeVecDyn_Size (RpVec<Entity> const& v);
		static void  EncodeVecDyn      (Enc& enc, RpVec<Entity> const& v);

		template <typename OptType>
		static sizet EncodeOpt_Size(OptType const& o)
		{
			if (o.Any())
				return EncodeVal_Size(o.Ref(), FldEncDisp::AsField);
			return 0;
		}

		template <typename OptType>
		static void EncodeOpt(Enc& enc, OptType const& o)
		{
			if (o.Any())
				EncodeVal(enc, o.Ref(), FldEncDisp::AsField);
		}

		template <typename VecType>
		static sizet EncodeVec_Size(VecType const& v)
		{
			sizet n = EncodeVarUInt64_Size(v.Len());
			for (sizet i=0; i!=v.Len(); ++i)
				n += EncodeVal_Size(v[i], FldEncDisp::AsField);
			return n;
		}

		template <typename VecType>
		static void EncodeVec(Enc& enc, VecType const& v)
		{
			EncodeVarUInt64(enc, v.Len());
			for (sizet i=0; i!=v.Len(); ++i)
				EncodeVal(enc, v[i], FldEncDisp::AsField);
		}

		sizet EncodeField_Size  (EntityFieldInfo const* efi, FldEncDisp::E fed) const;
		void  EncodeField       (Enc& enc, EntityFieldInfo const* efi, FldEncDisp::E fed) const; 

		sizet EncodeFields_Size () const;
		void  EncodeFields      (Enc& enc) const;

		uint EncodeOptFieldPresence(Enc& enc, EntityFieldInfo const* efi, uint currentlyEncodingVersion, uint lowestNextVersion) const;

		template <class ET,
			typename std::enable_if_t<!std::is_same<typename ET::KeyType, Str>::value, int> = 0>		// Use this version for all ET::KeyType except Str
		static void EncodeKey(Enc& enc, typename ET::KeyType const& v)
		{
			EnsureThrow(KeyCat::IsNonStrKey(ET::KeyField->m_keyCat));

			Enc::Meter meter = enc.IncMeter(4 + EncodeVal_Size(v, FldEncDisp::AsKey));
			EncodeUInt32(enc, ET::Kind);
			EncodeVal(enc, v, FldEncDisp::AsKey);
			EnsureThrow(meter.Met());
		}

		template <class ET,
			typename std::enable_if_t<std::is_same<typename ET::KeyType, Str>::value, int> = 0>		// Use this version for Str
		static void EncodeKey(Enc& enc, Str const& v)
		{
			EnsureThrow(KeyCat::IsStrKey(ET::KeyField->m_keyCat));

			Enc::Meter meter = enc.IncMeter(4 + EncodeVal_Size(v, FldEncDisp::AsKey));
			EncodeUInt32(enc, ET::Kind);

			if (KeyCat::IsSensitiveKey(ET::KeyField->m_keyCat))
				EncodeVal(enc, v, FldEncDisp::AsKey);
			else
				EncodeVal(enc, ToLower(v), FldEncDisp::AsKey);

			EnsureThrow(meter.Met());
		}

		template <class ET>
		static void EncodeKeyOpt(Enc& enc, typename ET::KeyType const* v)
		{
			EnsureThrow(ET::Kind + 1 > ET::Kind);

			if (v)
				EncodeKey<ET>(enc, *v);
			else
				EncodeEmptyKey(enc, ET::Kind + 1);
		}

	public:
		sizet EncodeKey_Size () const { return 4 + (HaveKey() ? EncodeField_Size(m_key, FldEncDisp::AsKey) : 0); }
		void  EncodeKey      (Enc& enc) const;

	protected:
		static void EncodeEmptyKey (Enc& enc, uint32 kind);
 
		static void DecodeValE(Seq& s, bool&   v, FldEncDisp::E    ) { uint b; EnsureThrow(DecodeByte(s, b)); v = (b != 0) && (b != '0'); }
		static void DecodeValE(Seq& s, Entity& v, FldEncDisp::E    ) { uint64 n; EnsureThrow(DecodeVarUInt64(s, n)); v.DecodeFieldsE(s, NumCast<sizet>(n)); }
		static void DecodeValE(Seq& s, uint32& v, FldEncDisp::E    ) { uint64 v64; EnsureThrow(DecodeVarUInt64(s, v64)); v = NumCast<uint32>(v64); }
		static void DecodeValE(Seq& s, uint64& v, FldEncDisp::E    ) { EnsureThrow(DecodeVarUInt64(s, v)); }
		static void DecodeValE(Seq& s, int64&  v, FldEncDisp::E    ) { EnsureThrow(DecodeVarSInt64(s, v)); }
		static void DecodeValE(Seq& s, double& v, FldEncDisp::E fed) { if (fed == FldEncDisp::AsKey) EnsureThrow(DecodeSortDouble(s, v)); else EnsureThrow(DecodeDouble(s, v)); }
		static void DecodeValE(Seq& s, Time&   v, FldEncDisp::E    ) { uint64 n; EnsureThrow(DecodeVarUInt64(s, n)); v = Time::FromFt(n); }
		static void DecodeValE(Seq& s, ObjId&  v, FldEncDisp::E    ) { EnsureThrow(DecodeVarUInt64(s, v.m_uniqueId) && DecodeVarUInt64(s, v.m_index)); }
		static void DecodeValE(Seq& s, Str&    v, FldEncDisp::E fed) { if (fed == FldEncDisp::AsKey) EnsureThrow(DecodeSortStr(s, v)); else { Seq data; EnsureThrow(DecodeVarStr(s, data)); v = data; } }

		static void DecodeOptDynE(Seq& s, Rp<Entity>& o);
		static void DecodeVecDynE(Seq& s, RpVec<Entity>& v);

		template <typename OptType>
		static void DecodeOptE(Seq& s, OptType& o)
		{
			// Assumes optional field is present. Must not be called if not present
			o.Init();
			DecodeValE(s, o.Ref(), FldEncDisp::AsField);
		}

		template <typename VecType>
		static void DecodeVecE(Seq& s, VecType& v)
		{
			v.Clear();
		
			uint64 n = 0;
			EnsureThrow(DecodeVarUInt64(s, n));
			v.ReserveExact(NumCast<sizet>(n));

			for (sizet i=0; i!=n; ++i)
				DecodeValE(s, v.Add(), FldEncDisp::AsField);
		}

		void DecodeFieldE(Seq& s, EntityFieldInfo const* efi, FldEncDisp::E fed);
		void DecodeFieldsE(Seq& s, sizet nrFieldsToDecode = SIZE_MAX);

		friend struct EntityChildInfo;
		friend class EntityStore;
	};



	template <class T> inline T&       FieldRef(Entity& e,       sizet offset) { return *((T*) (((byte*) &e) + offset)); }
	template <class T> inline T const& FieldRef(Entity const& e, sizet offset) { return *((T*) (((byte*) &e) + offset)); }



	template <class T>
	class EntOpt : public Opt<T>, private EntOptBase
	{
	public:
		static_assert(std::is_nothrow_destructible<T>::value, "Throwing destructor not supported by Opt<T> at time of this implementation");

		EntOptBase const* ToEntOptBase() const noexcept { return (EntOptBase const*) this; }

		template <typename... Args>
		T& Init(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
			{ return Opt<T>::Init<Args...>(std::forward<Args>(args)...); }

		EntOpt<T>&    Clear() noexcept                        { Opt<T>::Clear(); return *this; }
		bool          Any() const noexcept                    { return Opt<T>::Any(); }
		T&            Ref()                                   { return Opt<T>::Ref(); }
		T const&      Ref() const                             { return Opt<T>::Ref(); }

	protected:
		Entity&       EOB_Init() override final               { return Init();  }
		void          EOB_Clear() noexcept override final     {        Clear(); }
		bool          EOB_Any() const noexcept override final { return Any();   }
		Entity&       EOB_Ref() override final                { return Ref();   }
		Entity const& EOB_Ref() const override final          { return Ref();   }
	};



	template <class T>
	class EntVec : public Vec<T>, private EntVecBase
	{
	public:
		EntVecBase const* ToEntVecBase() const noexcept { return (EntVecBase const*) this; }

		EntVec<T>&    ReserveExact(sizet i)                    { Vec<T>::ReserveExact(i); return *this; }
		sizet         Len() const noexcept                     { return Vec<T>::Len(); }
		T&            operator[] (sizet i)                     { return Vec<T>::operator[](i); }
		T const&      operator[] (sizet i) const               { return Vec<T>::operator[](i); }
		EntVec<T>&    Clear() noexcept                         { Vec<T>::Clear(); return *this; }
		T&            Add()                                    { return Vec<T>::Add(); }

	protected:
		void          EVB_ReserveExact(sizet i) override final { ReserveExact(i); };
		sizet         EVB_Len() const noexcept override final  { return Len(); }
		Entity&       EVB_At(sizet i) override final           { return operator[](i); }
		Entity const& EVB_At(sizet i) const override final     { return operator[](i); }
		void          EVB_Clear() noexcept override final      { Clear(); }
		Entity&       EVB_Add() override final                 { return Add(); }
	};



	template <class T> struct EntityKeyReturnType {};
	template <>        struct EntityKeyReturnType<uint64> { using Type = uint64;     };
	template <>        struct EntityKeyReturnType<int64>  { using Type = int64;      };
	template <>        struct EntityKeyReturnType<double> { using Type = double;     };
	template <>        struct EntityKeyReturnType<Time>   { using Type = Time;       };
	template <>        struct EntityKeyReturnType<ObjId>  { using Type = ObjId;      };
	template <>        struct EntityKeyReturnType<Str>    { using Type = Str const&; };

	
	inline uint32 FieldTypeOf(Entity        const&) { return FieldType::Entity;   }
	inline uint32 FieldTypeOf(bool          const&) { return FieldType::Bool;     }
	inline uint32 FieldTypeOf(uint64        const&) { return FieldType::UInt;     }
	inline uint32 FieldTypeOf(int64         const&) { return FieldType::SInt;     }
	inline uint32 FieldTypeOf(double        const&) { return FieldType::Float;    }
	inline uint32 FieldTypeOf(Time          const&) { return FieldType::Time;     }
	inline uint32 FieldTypeOf(ObjId         const&) { return FieldType::ObjId;    }
	inline uint32 FieldTypeOf(Str           const&) { return FieldType::Str;      }

	inline uint32 FieldTypeOf(Rp<Entity>    const&) { return FieldType::OptDyn;   }
	inline uint32 FieldTypeOf(Opt<bool  >   const&) { return FieldType::OptBool;  }
	inline uint32 FieldTypeOf(Opt<uint64>   const&) { return FieldType::OptUInt;  }
	inline uint32 FieldTypeOf(Opt<int64 >   const&) { return FieldType::OptSInt;  }
	inline uint32 FieldTypeOf(Opt<double>   const&) { return FieldType::OptFloat; }
	inline uint32 FieldTypeOf(Opt<Time  >   const&) { return FieldType::OptTime;  }
	inline uint32 FieldTypeOf(Opt<ObjId >   const&) { return FieldType::OptObjId; }
	inline uint32 FieldTypeOf(Opt<Str   >   const&) { return FieldType::OptStr;   }

	inline uint32 FieldTypeOf(RpVec<Entity> const&) { return FieldType::VecDyn;   }
	inline uint32 FieldTypeOf(Vec<bool  >   const&) { return FieldType::VecBool;  }
	inline uint32 FieldTypeOf(Vec<uint64>   const&) { return FieldType::VecUInt;  }
	inline uint32 FieldTypeOf(Vec<int64 >   const&) { return FieldType::VecSInt;  }
	inline uint32 FieldTypeOf(Vec<double>   const&) { return FieldType::VecFloat; }
	inline uint32 FieldTypeOf(Vec<Time  >   const&) { return FieldType::VecTime;  }
	inline uint32 FieldTypeOf(Vec<ObjId >   const&) { return FieldType::VecObjId; }
	inline uint32 FieldTypeOf(Vec<Str   >   const&) { return FieldType::VecStr;   }

	template <class T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
		inline uint32 FieldTypeOf(T const&) { return FieldType::Enum; }

	template <class T> inline uint32 FieldTypeOf(EntOpt<T> const&) { return FieldType::OptEnt;   }
	template <class T> inline uint32 FieldTypeOf(EntVec<T> const&) { return FieldType::VecEnt;   }

	// Need to use these overloads so we get the right offset for objects derived from Entity
	inline sizet FieldOffsetOf(Entity        const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(bool          const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(uint64        const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(int64         const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(double        const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Time          const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(ObjId         const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Str           const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }

	inline sizet FieldOffsetOf(Rp<Entity>    const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<bool  >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<uint64>   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<int64 >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<double>   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<Time  >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<ObjId >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Opt<Str   >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }

	inline sizet FieldOffsetOf(RpVec<Entity> const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<bool  >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<uint64>   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<int64 >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<double>   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<Time  >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<ObjId >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }
	inline sizet FieldOffsetOf(Vec<Str   >   const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }

	template <class T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
		inline sizet FieldOffsetOf(T const& f) { return (sizet) (((byte const*) &f) - ((byte const*) nullptr)); }

	// Need to use this overload so we get the right EntOptBase/EntVecBase offset for EntOpt/EntVec objects
	template <class T> inline sizet FieldOffsetOf(EntOpt<T> const& v) { return (sizet) (((byte const*) v.ToEntOptBase()) - ((byte const*) nullptr)); }
	template <class T> inline sizet FieldOffsetOf(EntVec<T> const& v) { return (sizet) (((byte const*) v.ToEntVecBase()) - ((byte const*) nullptr)); }


	
	// Vec and Opt are friends to allow use of Vec<ENT> and Opt<ENT> using default constructor, without requiring Entity::Contained
	#define ENTITY_DECL_BEGIN(ENT)				class ENT : public Entity { \
														ENT(); \
														friend class VecCore<VecBaseHeap<ENT>>; \
														friend class Opt<ENT>; \
													public: \
														static uint32 const Kind; \
														static uint32 const NrFields; \
														static EntityFieldInfo const Fields[]; \
														static EntityFieldInfo const* KeyField; \
														ENT(EntityStore& store, ObjId parentId = ObjId()); \
														ENT(EChildOf, Entity const& parent); \
														ENT(EContainedOrTbd); \
														static Rp<Entity> Create(EntityStore* store, ObjId parentId); \
														static ENT const Sample;
	#define ENTITY_DECL_FIELD(TYPE, FLD)				TYPE    f_##FLD; enum { KC_##FLD = KeyCat::None };
	#define ENTITY_DECL_FLD_C(TYPE, FLD)				TYPE    f_##FLD { Contained }; enum { KC_##FLD = KeyCat::None };
	#define ENTITY_DECL_FLD_E(TYPE, FLD)				TYPE::E f_##FLD; using ET_##FLD = TYPE;
	#define ENTITY_DECL_FLD_K(TYPE, FLD, KC)			TYPE    f_##FLD; enum { KC_##FLD = (KC) }; At::EntityKeyReturnType<TYPE>::Type Key() const { return f_##FLD; } \
														using KeyType = TYPE;
	#define ENTITY_DECL_CLOSE()						};


	#define ENTITY_DEF_BEGIN(ENT)					uint32 const ENT::Kind = Crc32(#ENT); \
													EntityFieldInfo const ENT::Fields[] = {
	#define ENTITY_DEF_FIELD(ENT, FLD)					EntityFieldInfo(#FLD, At::FieldOffsetOf(((ENT const*) nullptr)->f_##FLD), At::FieldTypeOf(((ENT const*) nullptr)->f_##FLD), (KeyCat::E) ENT::KC_##FLD, 0),
	#define ENTITY_DEF_FLD_V(ENT, FLD, VER)				EntityFieldInfo(#FLD, At::FieldOffsetOf(((ENT const*) nullptr)->f_##FLD), At::FieldTypeOf(((ENT const*) nullptr)->f_##FLD), (KeyCat::E) ENT::KC_##FLD, (VER)),
	#define ENTITY_DEF_FLD_E(ENT, FLD)					EntityFieldInfo(#FLD, At::FieldOffsetOf(((ENT const*) nullptr)->f_##FLD), ENT::ET_##FLD::TypeName(), &ENT::ET_##FLD::Name, 0),
	#define ENTITY_DEF_F_E_V(ENT, FLD, VER)				EntityFieldInfo(#FLD, At::FieldOffsetOf(((ENT const*) nullptr)->f_##FLD), ENT::ET_##FLD::TypeName(), &ENT::ET_##FLD::Name, (VER)),
	#define ENTITY_DEF_CLOSE(ENT)						EntityFieldInfo() }; \
													EntityFieldInfo const* ENT::KeyField = nullptr; \
													uint32 const ENT::NrFields = VerifyEntityFields(ENT::Fields, ENT::KeyField); \
													ENT::ENT(EntityStore& store, ObjId parentId) : Entity(&store, Kind, #ENT, NrFields, ENT::Fields, ENT::KeyField, parentId) { ReInitFields(); } \
													ENT::ENT(EChildOf, Entity const& parent) : Entity(GetStorePtr(parent), Kind, #ENT, NrFields, ENT::Fields, ENT::KeyField, parent.m_entityId) { ReInitFields(); } \
													ENT::ENT(EContainedOrTbd) : Entity(0, Kind, #ENT, NrFields, ENT::Fields, ENT::KeyField, ObjId()) { ReInitFields(); } \
													ENT::ENT()                : Entity(0, Kind, #ENT, NrFields, ENT::Fields, ENT::KeyField, ObjId()) { ReInitFields(); } \
													Rp<Entity> ENT::Create(EntityStore* store, ObjId parentId) { return If(!store, Entity*, new ENT(), new ENT(*store, parentId)); } \
													ENT const ENT::Sample; \
													uint32 g_entityAdded##ENT = AddEntityKind(ENT::Sample, ENT::Create);



	template <class T>
	struct EntityByIdKey
	{
		using Val = Rp<T>;
		static ObjId GetKey(Rp<T> const& x) noexcept { return x->m_entityId; }
	};

	template <class T> using EntityByIdMap = MapCore<MapCritLess<EntityByIdKey<T>>>;
	template <class T> using EntityMap     = Map<Rp<T>>;
	template <class T> using EntityMapR    = MapR<Rp<T>>;

}
