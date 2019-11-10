#pragma once

#include "AtJsonReadWrite.h"
#include "AtStr.h"


namespace At
{

	class JsonBuilder
	{
	public:
		enum { DefaultExpectedSize = 4000 };
		JsonBuilder(sizet expectedSize = DefaultExpectedSize) { m_s.ReserveExact(expectedSize); }

		Seq Final() const { EnsureThrow(m_elems.Len() == 1); return m_s; }

		JsonBuilder& Array     () { OnValue(); m_elems.Push     (ElemType::Array);  m_s.Ch('['); return *this; }
		JsonBuilder& EndArray  () {            m_elems.PopExact (ElemType::Array);  m_s.Ch(']'); return *this; }

		JsonBuilder& Object    () { OnValue(); m_elems.Push     (ElemType::Object); m_s.Ch('{'); return *this; }
		JsonBuilder& EndObject () {            m_elems.PopExact (ElemType::Object); m_s.Ch('}'); return *this; }

		JsonBuilder& Member    (Seq name);

		JsonBuilder& String    (Seq    s)              { OnValue(); Json::EncodeString(m_s, s); return *this; }
		JsonBuilder& UInt      (uint64 v)              { OnValue(); m_s.UInt (v);               return *this; }
		JsonBuilder& SInt      (int64  v)              { OnValue(); m_s.SInt (v);               return *this; }
		JsonBuilder& Dbl       (double v, uint prec=4) { OnValue(); m_s.Dbl  (v, prec);         return *this; }

		JsonBuilder& True      () { return Literal("true"  ); }
		JsonBuilder& False     () { return Literal("false" ); }
		JsonBuilder& Null      () { return Literal("null"  ); }

	private:
		enum class ElemType { Root, Array, Object, Member };

		struct Elem
		{
			ElemType m_type        {};
			bool     m_anyChildren {};

			bool Is(ElemType t) const { return m_type == t; }
		};

		struct ElemStack : Vec<Elem>
		{
			// There's always at least one element of type ElemType::Root
			ElemStack() { ReserveExact(10); Add(); }

			bool LastIs   (ElemType t) const { return Last().Is(t); }
			void Push     (ElemType t)       { Add().m_type = t; }
			void PopExact (ElemType t)       { EnsureThrow(Len() >= 2); EnsureThrow(LastIs(t)); PopLast(); }
		};

		Str       m_s;
		ElemStack m_elems;

		void         OnValue();
		JsonBuilder& Literal(Seq lit) { OnValue(); m_s.Add(lit); return *this; }
	};

}
