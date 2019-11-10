#pragma once

#include "AtIncludes.h"
#include "AtNum.h"

namespace At
{
	// Bulk memory storage, designed for use by Locations, which needs to allocate
	// a tremendous number of objects which later need to be freed all at the same time.
	// When using the standard allocator, freeing all the objects takes minutes even in
	// release mode.
	//
	// This implementation takes advantage of knowing that all the objects will only
	// ever need to be freed at the same time, so deallocation of an individual object
	// does nothing, but memory for many objects is released when destroying BulkStorage.
	//
	// Locations doesn't need to allocate large objects, so the maximum allocation size
	// is low to simplify implementation. Support for larger allocations could be added
	// by e.g. falling back to the standard allocator, but that's not needed at this point.

	struct BulkStorageBlock
	{
		enum { BlockSize = 1024 * 1024, MaxAlloc = 4096 };

		BulkStorageBlock* next;
		byte* remainingPtr;
		sizet remainingNr;
	
		// Block data follows this struct in memory.
	};


	class BulkStorage : NoCopy
	{
	public:
		~BulkStorage();
	
		BulkStorage& Self() { return *this; }
		byte* Allocate(sizet size);

	private:	
		BulkStorageBlock* m_first {};
		BulkStorageBlock* m_last  {};
	};


	template <class T>
	class BulkAlloc
	{
	public:
		typedef T const*	const_pointer;
		typedef T const&	const_reference;
		typedef ptrdiff		difference_type;
		typedef T*			pointer;
		typedef T&			reference;
		typedef sizet		size_type;
		typedef T			value_type;

	public:
		// Must be public so it's accessible to other BulkAlloc<T> types
		BulkStorage* m_storage;

	public:
		BulkAlloc(BulkStorage& storage) : m_storage(&storage) {}
	
		template <class Other>
		BulkAlloc(BulkAlloc<Other> const& x) : m_storage(x.m_storage) {}
	
		template <class Other>
		BulkAlloc& operator=(BulkAlloc<Other> const& x) { m_storage = x.m_storage; }	

		pointer		  address(reference		  value) const { return &value; }
		const_pointer address(const_reference value) const { return &value; }

		pointer allocate(size_type count)
			{ return (pointer) m_storage->Allocate(count * sizeof(T)); }
	
		template <class Other>
		pointer allocate(size_type count, Other const*)
			{ return (pointer) m_storage->Allocate(count * sizeof(T)); }
	
		void deallocate(pointer, size_type) {}
	
		void construct(pointer ptr, T const& value) { ::new (ptr) T(value); }
		void destroy(pointer ptr) { if (ptr) ptr->~T(); }	// VS 2013: when compiling with /W4, need to explicitly use 'ptr' to avoid spurious C4100 "unreferenced formal parameter" warning
	
		size_type max_size() const { return SIZE_MAX / sizeof(T); }
	
		template <class Other>
		struct rebind { typedef BulkAlloc<Other> other; };
	};
}
