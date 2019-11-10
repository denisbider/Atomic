#pragma once

#include "AtEnsure.h"

namespace At
{

	template <class It>
	class ConstIt : protected It
	{
	public:
		using Val = typename It::Val;

		template <typename... Args>
		ConstIt(Args&&... args) : It(std::forward<Args>(args)...) {}

		ConstIt<It>& operator= (It const& x) noexcept { It::operator=(x); return *this; }

		bool Valid () const noexcept { return It::Valid(); }
		bool Any   () const noexcept { return It::Any(); }

		Val const* Ptr() const { return It::Ptr(); }
		Val const& Ref() const { return It::Ref(); }

		Val const& operator*  () const { return It::operator* (); }
		Val const* operator-> () const { return It::operator->(); }

		bool operator== (It          const& x) const noexcept { return It::operator==(x); }
		bool operator== (ConstIt<It> const& x) const noexcept { return It::operator==(x); }
		bool operator!= (It          const& x) const noexcept { return It::operator!=(x); }
		bool operator!= (ConstIt<It> const& x) const noexcept { return It::operator!=(x); }

		ConstIt<It>& operator++ ()    { return Increment(); }
		ConstIt<It>& operator-- ()    { return Decrement(); }
		ConstIt<It>  operator++ (int) { ConstIt<It> prev{*this}; It::Increment(); return prev; }
		ConstIt<It>  operator-- (int) { ConstIt<It> prev{*this}; It::Decrement(); return prev; }
		
		ConstIt<It>& Increment() { It::Increment(); return *this; }
		ConstIt<It>& Decrement() { It::Decrement(); return *this; }
	};

}
