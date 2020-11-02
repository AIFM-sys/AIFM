/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

/* Container implementations in this header are based on PPL implementations 
   provided by Microsoft. */

#ifndef __TBB_concurrent_unordered_internal_H
#define __TBB_concurrent_unordered_internal_H

#include "tbb_stddef.h"

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <iterator>
#include <utility>      // Need std::pair
#include <functional>
#include <string>       // For tbb_hasher
#include <cstring>      // Need std::memset

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

#include "tbb_machine.h"
#include "tbb_exception.h"
#include "tbb_allocator.h"

namespace tbb {
namespace interface5 {
//! @cond INTERNAL
namespace internal {

template <typename T, typename Allocator>
class split_ordered_list;
template <typename Traits>
class concurrent_unordered_base;

// Forward list iterators (without skipping dummy elements)
template<class Solist, typename Value>
class flist_iterator : public std::iterator<std::forward_iterator_tag, Value>
{
    template <typename T, typename Allocator>
    friend class split_ordered_list;
    template <typename Traits>
    friend class concurrent_unordered_base;
    template<class M, typename V>
    friend class flist_iterator;

    typedef typename Solist::nodeptr_t nodeptr_t;
public:
    typedef typename Solist::value_type value_type;
    typedef typename Solist::difference_type difference_type;
    typedef typename Solist::pointer pointer;
    typedef typename Solist::reference reference;

    flist_iterator() : my_node_ptr(0) {}
    flist_iterator( const flist_iterator<Solist, typename Solist::value_type> &other )
        : my_node_ptr(other.my_node_ptr) {}

    reference operator*() const { return my_node_ptr->my_element; }
    pointer operator->() const { return &**this; }

    flist_iterator& operator++() {
        my_node_ptr = my_node_ptr->my_next;
        return *this;
    }

    flist_iterator operator++(int) {
        flist_iterator tmp = *this;
        ++*this;
        return tmp;
    }

protected:
    flist_iterator(nodeptr_t pnode) : my_node_ptr(pnode) {}
    nodeptr_t get_node_ptr() const { return my_node_ptr; }

    nodeptr_t my_node_ptr;

    template<typename M, typename T, typename U>
    friend bool operator==( const flist_iterator<M,T> &i, const flist_iterator<M,U> &j );
    template<typename M, typename T, typename U>
    friend bool operator!=( const flist_iterator<M,T>& i, const flist_iterator<M,U>& j );
};

template<typename Solist, typename T, typename U>
bool operator==( const flist_iterator<Solist,T> &i, const flist_iterator<Solist,U> &j ) {
    return i.my_node_ptr == j.my_node_ptr;
}
template<typename Solist, typename T, typename U>
bool operator!=( const flist_iterator<Solist,T>& i, const flist_iterator<Solist,U>& j ) {
    return i.my_node_ptr != j.my_node_ptr;
}

// Split-order list iterators, needed to skip dummy elements
template<class Solist, typename Value>
class solist_iterator : public flist_iterator<Solist, Value>
{
    typedef flist_iterator<Solist, Value> base_type;
    typedef typename Solist::nodeptr_t nodeptr_t;
    using base_type::get_node_ptr;
    template <typename T, typename Allocator>
    friend class split_ordered_list;
    template<class M, typename V>
    friend class solist_iterator;
    template<typename M, typename T, typename U>
    friend bool operator==( const solist_iterator<M,T> &i, const solist_iterator<M,U> &j );
    template<typename M, typename T, typename U>
    friend bool operator!=( const solist_iterator<M,T>& i, const solist_iterator<M,U>& j );

    const Solist *my_list_ptr;
    solist_iterator(nodeptr_t pnode, const Solist *plist) : base_type(pnode), my_list_ptr(plist) {}

public:
    typedef typename Solist::value_type value_type;
    typedef typename Solist::difference_type difference_type;
    typedef typename Solist::pointer pointer;
    typedef typename Solist::reference reference;

    solist_iterator() {}
    solist_iterator(const solist_iterator<Solist, typename Solist::value_type> &other )
        : base_type(other), my_list_ptr(other.my_list_ptr) {}

    reference operator*() const {
        return this->base_type::operator*();
    }

    pointer operator->() const {
        return (&**this);
    }

    solist_iterator& operator++() {
        do ++(*(base_type *)this);
        while (get_node_ptr() != NULL && get_node_ptr()->is_dummy());

        return (*this);
    }

    solist_iterator operator++(int) {
        solist_iterator tmp = *this;
        do ++*this;
        while (get_node_ptr() != NULL && get_node_ptr()->is_dummy());

        return (tmp);
    }
};

template<typename Solist, typename T, typename U>
bool operator==( const solist_iterator<Solist,T> &i, const solist_iterator<Solist,U> &j ) {
    return i.my_node_ptr == j.my_node_ptr && i.my_list_ptr == j.my_list_ptr;
}
template<typename Solist, typename T, typename U>
bool operator!=( const solist_iterator<Solist,T>& i, const solist_iterator<Solist,U>& j ) {
    return i.my_node_ptr != j.my_node_ptr || i.my_list_ptr != j.my_list_ptr;
}

// Forward type and class definitions
typedef size_t sokey_t;

// Forward list in which elements are sorted in a split-order
template <typename T, typename Allocator>
class split_ordered_list
{
public:
    typedef split_ordered_list<T, Allocator> self_type;
    typedef typename Allocator::template rebind<T>::other allocator_type;
    struct node;
    typedef node *nodeptr_t;

    typedef typename allocator_type::size_type size_type;
    typedef typename allocator_type::difference_type difference_type;
    typedef typename allocator_type::pointer pointer;
    typedef typename allocator_type::const_pointer const_pointer;
    typedef typename allocator_type::reference reference;
    typedef typename allocator_type::const_reference const_reference;
    typedef typename allocator_type::value_type value_type;

    typedef solist_iterator<self_type, const value_type> const_iterator;
    typedef solist_iterator<self_type, value_type> iterator;
    typedef flist_iterator<self_type, const value_type> raw_const_iterator;
    typedef flist_iterator<self_type, value_type> raw_iterator;

    // Node that holds the element in a split-ordered list
    struct node : tbb::internal::no_assign
    {
        // Initialize the node with the given order key
        void init(sokey_t order_key) {
            my_order_key = order_key;
            my_next = NULL;
        }

        // Return the order key (needed for hashing)
        sokey_t get_order_key() const { // TODO: remove
            return my_order_key;
        }

        // Inserts the new element in the list in an atomic fashion
        nodeptr_t atomic_set_next(nodeptr_t new_node, nodeptr_t current_node)
        {
            // Try to change the next pointer on the current element to a new element, only if it still points to the cached next
            nodeptr_t exchange_node = (nodeptr_t) __TBB_CompareAndSwapW((void *) &my_next, (uintptr_t)new_node, (uintptr_t)current_node);

            if (exchange_node == current_node) // TODO: why this branch?
            {
                // Operation succeeded, return the new node
                return new_node;
            }
            else
            {
                // Operation failed, return the "interfering" node
                return exchange_node;
            }
        }

        // Checks if this element in the list is a dummy, order enforcing node. Dummy nodes are used by buckets
        // in the hash table to quickly index into the right subsection of the split-ordered list.
        bool is_dummy() const {
            return (my_order_key & 0x1) == 0;
        }


        nodeptr_t  my_next;      // Next element in the list
        value_type my_element;   // Element storage
        sokey_t    my_order_key; // Order key for this element
    };

    // Allocate a new node with the given order key and value
    nodeptr_t create_node(sokey_t order_key, const T &value) {
        nodeptr_t pnode = my_node_allocator.allocate(1);

        __TBB_TRY {
            new(static_cast<void*>(&pnode->my_element)) T(value);
            pnode->init(order_key);
        } __TBB_CATCH(...) {
            my_node_allocator.deallocate(pnode, 1);
            __TBB_RETHROW();
        }

        return (pnode);
    }

    // Allocate a new node with the given order key; used to allocate dummy nodes
    nodeptr_t create_node(sokey_t order_key) {
        nodeptr_t pnode = my_node_allocator.allocate(1);

        __TBB_TRY {
            new(static_cast<void*>(&pnode->my_element)) T();
            pnode->init(order_key);
        } __TBB_CATCH(...) {
            my_node_allocator.deallocate(pnode, 1);
            __TBB_RETHROW();
        }

        return (pnode);
    }

   split_ordered_list(allocator_type a = allocator_type())
       : my_node_allocator(a), my_element_count(0)
    {
        // Immediately allocate a dummy node with order key of 0. This node
        // will always be the head of the list.
        my_head = create_node(0);
    }

    ~split_ordered_list()
    {
        // Clear the list
        clear();

        // Remove the head element which is not cleared by clear()
        nodeptr_t pnode = my_head;
        my_head = NULL;

        __TBB_ASSERT(pnode != NULL && pnode->my_next == NULL, "Invalid head list node");

        destroy_node(pnode);
    }

    // Common forward list functions

    allocator_type get_allocator() const {
        return (my_node_allocator);
    }

    void clear() {
        nodeptr_t pnext;
        nodeptr_t pnode = my_head;

        __TBB_ASSERT(my_head != NULL, "Invalid head list node");
        pnext = pnode->my_next;
        pnode->my_next = NULL;
        pnode = pnext;

        while (pnode != NULL)
        {
            pnext = pnode->my_next;
            destroy_node(pnode);
            pnode = pnext;
        }

        my_element_count = 0;
    }

    // Returns a first non-dummy element in the SOL
    iterator begin() {
        return first_real_iterator(raw_begin());
    }

    // Returns a first non-dummy element in the SOL
    const_iterator begin() const {
        return first_real_iterator(raw_begin());
    }

    iterator end() {
        return (iterator(0, this));
    }

    const_iterator end() const {
        return (const_iterator(0, this));
    }

    const_iterator cbegin() const {
        return (((const self_type *)this)->begin());
    }

    const_iterator cend() const {
        return (((const self_type *)this)->end());
    }

    // Checks if the number of elements (non-dummy) is 0
    bool empty() const {
        return (my_element_count == 0);
    }

    // Returns the number of non-dummy elements in the list
    size_type size() const {
        return my_element_count;
    }

    // Returns the maximum size of the list, determined by the allocator
    size_type max_size() const {
        return my_node_allocator.max_size();
    }

    // Swaps 'this' list with the passed in one
    void swap(self_type& other)
    {
        if (this == &other)
        {
            // Nothing to do
            return;
        }

        std::swap(my_element_count, other.my_element_count);
        std::swap(my_head, other.my_head);
    }

    // Split-order list functions

    // Returns a first element in the SOL, which is always a dummy
    raw_iterator raw_begin() {
        return raw_iterator(my_head);
    }

    // Returns a first element in the SOL, which is always a dummy
    raw_const_iterator raw_begin() const {
        return raw_const_iterator(my_head);
    }

    raw_iterator raw_end() {
        return raw_iterator(0);
    }

    raw_const_iterator raw_end() const {
        return raw_const_iterator(0);
    }

    static sokey_t get_order_key(const raw_const_iterator& it) {
        return it.get_node_ptr()->get_order_key();
    }

    static sokey_t get_safe_order_key(const raw_const_iterator& it) {
        if( !it.get_node_ptr() ) return sokey_t(~0U);
        return it.get_node_ptr()->get_order_key();
    }

    // Returns a public iterator version of the internal iterator. Public iterator must not
    // be a dummy private iterator.
    iterator get_iterator(raw_iterator it) {
        __TBB_ASSERT(it.get_node_ptr() == NULL || !it.get_node_ptr()->is_dummy(), "Invalid user node (dummy)");
        return iterator(it.get_node_ptr(), this);
    }

    // Returns a public iterator version of the internal iterator. Public iterator must not
    // be a dummy private iterator.
    const_iterator get_iterator(raw_const_iterator it) const {
        __TBB_ASSERT(it.get_node_ptr() == NULL || !it.get_node_ptr()->is_dummy(), "Invalid user node (dummy)");
        return const_iterator(it.get_node_ptr(), this);
    }

    // Returns a non-const version of the raw_iterator
    raw_iterator get_iterator(raw_const_iterator it) {
        return raw_iterator(it.get_node_ptr());
    }

    // Returns a non-const version of the iterator
    static iterator get_iterator(const_iterator it) {
        return iterator(it.my_node_ptr, it.my_list_ptr);
    }

    // Returns a public iterator version of a first non-dummy internal iterator at or after
    // the passed in internal iterator.
    iterator first_real_iterator(raw_iterator it)
    {
        // Skip all dummy, internal only iterators
        while (it != raw_end() && it.get_node_ptr()->is_dummy())
            ++it;

        return iterator(it.get_node_ptr(), this);
    }

    // Returns a public iterator version of a first non-dummy internal iterator at or after
    // the passed in internal iterator.
    const_iterator first_real_iterator(raw_const_iterator it) const
    {
        // Skip all dummy, internal only iterators
        while (it != raw_end() && it.get_node_ptr()->is_dummy())
            ++it;

        return const_iterator(it.get_node_ptr(), this);
    }

    // Erase an element using the allocator
    void destroy_node(nodeptr_t pnode) {
        my_node_allocator.destroy(pnode);
        my_node_allocator.deallocate(pnode, 1);
    }

    // Try to insert a new element in the list. If insert fails, return the node that
    // was inserted instead.
    nodeptr_t try_insert(nodeptr_t previous, nodeptr_t new_node, nodeptr_t current_node) {
        new_node->my_next = current_node;
        return previous->atomic_set_next(new_node, current_node);
    }

    // Insert a new element between passed in iterators
    std::pair<iterator, bool> try_insert(raw_iterator it, raw_iterator next, const value_type &value, sokey_t order_key, size_type *new_count)
    {
        nodeptr_t pnode = create_node(order_key, value);
        nodeptr_t inserted_node = try_insert(it.get_node_ptr(), pnode, next.get_node_ptr());

        if (inserted_node == pnode)
        {
            // If the insert succeeded, check that the order is correct and increment the element count
            check_range();
            *new_count = __TBB_FetchAndAddW((uintptr_t*)&my_element_count, uintptr_t(1));
            return std::pair<iterator, bool>(iterator(pnode, this), true);
        }
        else
        {
            // If the insert failed (element already there), then delete the new one
            destroy_node(pnode);
            return std::pair<iterator, bool>(end(), false);
        }
    }

    // Insert a new dummy element, starting search at a parent dummy element
    raw_iterator insert_dummy(raw_iterator it, sokey_t order_key)
    {
        raw_iterator last = raw_end();
        raw_iterator where = it;

        __TBB_ASSERT(where != last, "Invalid head node");

        ++where;

        // Create a dummy element up front, even though it may be discarded (due to concurrent insertion)
        nodeptr_t dummy_node = create_node(order_key);

        for (;;)
        {
            __TBB_ASSERT(it != last, "Invalid head list node");

            // If the head iterator is at the end of the list, or past the point where this dummy
            // node needs to be inserted, then try to insert it.
            if (where == last || get_order_key(where) > order_key)
            {
                __TBB_ASSERT(get_order_key(it) < order_key, "Invalid node order in the list");

                // Try to insert it in the right place
                nodeptr_t inserted_node = try_insert(it.get_node_ptr(), dummy_node, where.get_node_ptr());

                if (inserted_node == dummy_node)
                {
                    // Insertion succeeded, check the list for order violations
                    check_range();
                    return raw_iterator(dummy_node);
                }
                else
                {
                    // Insertion failed: either dummy node was inserted by another thread, or
                    // a real element was inserted at exactly the same place as dummy node.
                    // Proceed with the search from the previous location where order key was
                    // known to be larger (note: this is legal only because there is no safe
                    // concurrent erase operation supported).
                    where = it;
                    ++where;
                    continue;
                }
            }
            else if (get_order_key(where) == order_key)
            {
                // Another dummy node with the same value found, discard the new one.
                destroy_node(dummy_node);
                return where;
            }

            // Move the iterator forward
            it = where;
            ++where;
        }

    }

    // This erase function can handle both real and dummy nodes
    void erase_node(raw_iterator previous, raw_const_iterator& where)
    {
        nodeptr_t pnode = (where++).get_node_ptr();
        nodeptr_t prevnode = previous.get_node_ptr();
        __TBB_ASSERT(prevnode->my_next == pnode, "Erase must take consecutive iterators");
        prevnode->my_next = pnode->my_next;

        destroy_node(pnode);
    }

    // Erase the element (previous node needs to be passed because this is a forward only list)
    iterator erase_node(raw_iterator previous, const_iterator where)
    {
        raw_const_iterator it = where;
        erase_node(previous, it);
        my_element_count--;

        return get_iterator(first_real_iterator(it));
    }

    // Move all elements from the passed in split-ordered list to this one
    void move_all(self_type& source)
    {
        raw_const_iterator first = source.raw_begin();
        raw_const_iterator last = source.raw_end();

        if (first == last)
            return;

        nodeptr_t previous_node = my_head;
        raw_const_iterator begin_iterator = first++;

        // Move all elements one by one, including dummy ones
        for (raw_const_iterator it = first; it != last;)
        {
            nodeptr_t pnode = it.get_node_ptr();

            nodeptr_t dummy_node = pnode->is_dummy() ? create_node(pnode->get_order_key()) : create_node(pnode->get_order_key(), pnode->my_element);
            previous_node = try_insert(previous_node, dummy_node, NULL);
            __TBB_ASSERT(previous_node != NULL, "Insertion must succeed");
            raw_const_iterator where = it++;
            source.erase_node(get_iterator(begin_iterator), where);
        }
        check_range();
    }


private:

    // Check the list for order violations
    void check_range()
    {
#if TBB_USE_ASSERT
        for (raw_iterator it = raw_begin(); it != raw_end(); ++it)
        {
            raw_iterator next_iterator = it;
            ++next_iterator;

            __TBB_ASSERT(next_iterator == end() || next_iterator.get_node_ptr()->get_order_key() >= it.get_node_ptr()->get_order_key(), "!!! List order inconsistency !!!");
        }
#endif
    }

    typename allocator_type::template rebind<node>::other my_node_allocator;  // allocator object for nodes
    size_type                                             my_element_count;   // Total item count, not counting dummy nodes
    nodeptr_t                                             my_head;            // pointer to head node
};

// Template class for hash compare
template<typename Key, typename Hasher, typename Key_equality>
class hash_compare
{
public:
    hash_compare() {}

    hash_compare(Hasher a_hasher) : my_hash_object(a_hasher) {}

    hash_compare(Hasher a_hasher, Key_equality a_keyeq) : my_hash_object(a_hasher), my_key_compare_object(a_keyeq) {}

    size_t operator()(const Key& key) const {
        return ((size_t)my_hash_object(key));
    }

    bool operator()(const Key& key1, const Key& key2) const {
        return (!my_key_compare_object(key1, key2));
    }

    Hasher       my_hash_object;        // The hash object
    Key_equality my_key_compare_object; // The equality comparator object
};

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127) // warning 4127 -- while (true) has a constant expression in it (for allow_multimapping)
#endif

template <typename Traits>
class concurrent_unordered_base : public Traits
{
protected:
    // Type definitions
    typedef concurrent_unordered_base<Traits> self_type;
    typedef typename Traits::value_type value_type;
    typedef typename Traits::key_type key_type;
    typedef typename Traits::hash_compare hash_compare;
    typedef typename Traits::value_compare value_compare;
    typedef typename Traits::allocator_type allocator_type;
    typedef typename allocator_type::pointer pointer;
    typedef typename allocator_type::const_pointer const_pointer;
    typedef typename allocator_type::reference reference;
    typedef typename allocator_type::const_reference const_reference;
    typedef typename allocator_type::size_type size_type;
    typedef typename allocator_type::difference_type difference_type;
    typedef split_ordered_list<value_type, typename Traits::allocator_type> solist_t;
    typedef typename solist_t::nodeptr_t nodeptr_t;
    // Iterators that walk the entire split-order list, including dummy nodes
    typedef typename solist_t::raw_iterator raw_iterator;
    typedef typename solist_t::raw_const_iterator raw_const_iterator;
    typedef typename solist_t::iterator iterator; // TODO: restore const iterator for unordered_sets
    typedef typename solist_t::const_iterator const_iterator;
    typedef iterator local_iterator;
    typedef const_iterator const_local_iterator;
    using Traits::my_hash_compare;
    using Traits::get_key;
    using Traits::allow_multimapping;

private:
    typedef std::pair<iterator, iterator> pairii_t;
    typedef std::pair<const_iterator, const_iterator> paircc_t;

    static size_type const pointers_per_table = sizeof(size_type) * 8;              // One bucket segment per bit
    static const size_type initial_bucket_number = 8;                               // Initial number of buckets
    static const size_type initial_bucket_load = 4;                                // Initial maximum number of elements per bucket

protected:
    // Constructors/Destructors
    concurrent_unordered_base(size_type n_of_buckets = initial_bucket_number,
        const hash_compare& hc = hash_compare(), const allocator_type& a = allocator_type())
        : Traits(hc), my_number_of_buckets(n_of_buckets), my_solist(a),
          my_allocator(a), my_maximum_bucket_size((float) initial_bucket_load)
    {
        internal_init();
    }

    concurrent_unordered_base(const concurrent_unordered_base& right, const allocator_type& a)
        : Traits(right.my_hash_compare), my_solist(a), my_allocator(a)
    {
        internal_copy(right);
    }

    concurrent_unordered_base(const concurrent_unordered_base& right)
        : Traits(right.my_hash_compare), my_solist(right.get_allocator()), my_allocator(right.get_allocator())
    {
        internal_init();
        internal_copy(right);
    }

    concurrent_unordered_base& operator=(const concurrent_unordered_base& right) {
        if (this != &right)
            internal_copy(right);
        return (*this);
    }

    ~concurrent_unordered_base() {
        // Delete all node segments
        internal_clear();
    }

public:
    allocator_type get_allocator() const {
        return my_solist.get_allocator();
    }

    // Size and capacity function
    bool empty() const {
        return my_solist.empty();
    }

    size_type size() const {
        return my_solist.size();
    }

    size_type max_size() const {
        return my_solist.max_size();
    }

    // Iterators 
    iterator begin() {
        return my_solist.begin();
    }

    const_iterator begin() const {
        return my_solist.begin();
    }

    iterator end() {
        return my_solist.end();
    }

    const_iterator end() const {
        return my_solist.end();
    }

    const_iterator cbegin() const {
        return my_solist.cbegin();
    }

    const_iterator cend() const {
        return my_solist.cend();
    }

    // Parallel traversal support
    class const_range_type : tbb::internal::no_assign {
        const concurrent_unordered_base &my_table;
        raw_const_iterator my_begin_node;
        raw_const_iterator my_end_node;
        mutable raw_const_iterator my_midpoint_node;
    public:
        //! Type for size of a range
        typedef typename concurrent_unordered_base::size_type size_type;
        typedef typename concurrent_unordered_base::value_type value_type;
        typedef typename concurrent_unordered_base::reference reference;
        typedef typename concurrent_unordered_base::difference_type difference_type;
        typedef typename concurrent_unordered_base::const_iterator iterator;

        //! True if range is empty.
        bool empty() const {return my_begin_node == my_end_node;}

        //! True if range can be partitioned into two subranges.
        bool is_divisible() const {
            return my_midpoint_node != my_end_node;
        }
        //! Split range.
        const_range_type( const_range_type &r, split ) : 
            my_table(r.my_table), my_end_node(r.my_end_node)
        {
            r.my_end_node = my_begin_node = r.my_midpoint_node;
            __TBB_ASSERT( !empty(), "Splitting despite the range is not divisible" );
            __TBB_ASSERT( !r.empty(), "Splitting despite the range is not divisible" );
            set_midpoint();
            r.set_midpoint();
        }
        //! Init range with container and grainsize specified
        const_range_type( const concurrent_unordered_base &a_table ) : 
            my_table(a_table), my_begin_node(a_table.my_solist.begin()),
            my_end_node(a_table.my_solist.end())
        {
            set_midpoint();
        }
        iterator begin() const { return my_table.my_solist.get_iterator(my_begin_node); }
        iterator end() const { return my_table.my_solist.get_iterator(my_end_node); }
        //! The grain size for this range.
        size_type grainsize() const { return 1; }

        //! Set my_midpoint_node to point approximately half way between my_begin_node and my_end_node.
        void set_midpoint() const {
            if( my_begin_node == my_end_node ) // not divisible
                my_midpoint_node = my_end_node;
            else {
                sokey_t begin_key = solist_t::get_safe_order_key(my_begin_node);
                sokey_t end_key = solist_t::get_safe_order_key(my_end_node);
                size_t mid_bucket = __TBB_ReverseBits( begin_key + (end_key-begin_key)/2 ) % my_table.my_number_of_buckets;
                while ( !my_table.is_initialized(mid_bucket) ) mid_bucket = my_table.get_parent(mid_bucket);
                my_midpoint_node = my_table.my_solist.first_real_iterator(my_table.get_bucket( mid_bucket ));
                if( my_midpoint_node == my_begin_node )
                    my_midpoint_node = my_end_node;
#if TBB_USE_ASSERT
                else {
                    sokey_t mid_key = solist_t::get_safe_order_key(my_midpoint_node);
                    __TBB_ASSERT( begin_key < mid_key, "my_begin_node is after my_midpoint_node" );
                    __TBB_ASSERT( mid_key <= end_key, "my_midpoint_node is after my_end_node" );
                }
#endif // TBB_USE_ASSERT
            }
        }
    };

    class range_type : public const_range_type {
    public:
        typedef typename concurrent_unordered_base::iterator iterator;
        //! Split range.
        range_type( range_type &r, split ) : const_range_type( r, split() ) {}
        //! Init range with container and grainsize specified
        range_type( const concurrent_unordered_base &a_table ) : const_range_type(a_table) {}

        iterator begin() const { return solist_t::get_iterator( const_range_type::begin() ); }
        iterator end() const { return solist_t::get_iterator( const_range_type::end() ); }
    };

    range_type range() {
        return range_type( *this );
    }

    const_range_type range() const {
        return const_range_type( *this );
    }

    // Modifiers
    std::pair<iterator, bool> insert(const value_type& value) {
        return internal_insert(value);
    }

    iterator insert(const_iterator, const value_type& value) {
        // Ignore hint
        return insert(value).first;
    }

    template<class Iterator>
    void insert(Iterator first, Iterator last) {
        for (Iterator it = first; it != last; ++it)
            insert(*it);
    }

    iterator unsafe_erase(const_iterator where) {
        return internal_erase(where);
    }

    iterator unsafe_erase(const_iterator first, const_iterator last) {
        while (first != last)
            unsafe_erase(first++);
        return my_solist.get_iterator(first);
    }

    size_type unsafe_erase(const key_type& key) {
        pairii_t where = equal_range(key);
        size_type item_count = internal_distance(where.first, where.second);
        unsafe_erase(where.first, where.second);
        return item_count;
    }

    void swap(concurrent_unordered_base& right) {
        if (this != &right) {
            std::swap(my_hash_compare, right.my_hash_compare); // TODO: check what ADL meant here
            my_solist.swap(right.my_solist);
            internal_swap_buckets(right);
            std::swap(my_number_of_buckets, right.my_number_of_buckets);
            std::swap(my_maximum_bucket_size, right.my_maximum_bucket_size);
        }
    }

    // Observers
    void clear() {
        // Clear list
        my_solist.clear();

        // Clear buckets
        internal_clear();
    }

    // Lookup
    iterator find(const key_type& key) {
        return internal_find(key);
    }

    const_iterator find(const key_type& key) const {
        return const_cast<self_type*>(this)->internal_find(key);
    }

    size_type count(const key_type& key) const {
        paircc_t answer = equal_range(key);
        size_type item_count = internal_distance(answer.first, answer.second);
        return item_count;
    }

    std::pair<iterator, iterator> equal_range(const key_type& key) {
        return internal_equal_range(key);
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return internal_equal_range(key);
    }

    // Bucket interface - for debugging 
    size_type unsafe_bucket_count() const {
        return my_number_of_buckets;
    }

    size_type unsafe_max_bucket_count() const {
        return segment_size(pointers_per_table-1);
    }

    size_type unsafe_bucket_size(size_type bucket) {
        size_type item_count = 0;
        if (is_initialized(bucket)) {
            raw_iterator it = get_bucket(bucket);
            ++it;
            for (; it != my_solist.raw_end() && !it.get_node_ptr()->is_dummy(); ++it)
                ++item_count;
        }
        return item_count;
    }

    size_type unsafe_bucket(const key_type& key) const {
        sokey_t order_key = (sokey_t) my_hash_compare(key);
        size_type bucket = order_key % my_number_of_buckets;
        return bucket;
    }

    // If the bucket is initialized, return a first non-dummy element in it
    local_iterator unsafe_begin(size_type bucket) {
        if (!is_initialized(bucket))
            return end();

        raw_iterator it = get_bucket(bucket);
        return my_solist.first_real_iterator(it);
    }

    // If the bucket is initialized, return a first non-dummy element in it
    const_local_iterator unsafe_begin(size_type bucket) const
    {
        if (!is_initialized(bucket))
            return end();

        raw_const_iterator it = get_bucket(bucket);
        return my_solist.first_real_iterator(it);
    }

    // @REVIEW: Takes O(n)
    // Returns the iterator after the last non-dummy element in the bucket
    local_iterator unsafe_end(size_type bucket)
    {
        if (!is_initialized(bucket))
            return end();

        raw_iterator it = get_bucket(bucket);
    
        // Find the end of the bucket, denoted by the dummy element
        do ++it;
        while(it != my_solist.raw_end() && !it.get_node_ptr()->is_dummy());

        // Return the first real element past the end of the bucket
        return my_solist.first_real_iterator(it);
    }

    // @REVIEW: Takes O(n)
    // Returns the iterator after the last non-dummy element in the bucket
    const_local_iterator unsafe_end(size_type bucket) const
    {
        if (!is_initialized(bucket))
            return end();

        raw_const_iterator it = get_bucket(bucket);
    
        // Find the end of the bucket, denoted by the dummy element
        do ++it;
        while(it != my_solist.raw_end() && !it.get_node_ptr()->is_dummy());

        // Return the first real element past the end of the bucket
        return my_solist.first_real_iterator(it);
    }

    const_local_iterator unsafe_cbegin(size_type bucket) const {
        return ((const self_type *) this)->begin();
    }

    const_local_iterator unsafe_cend(size_type bucket) const {
        return ((const self_type *) this)->end();
    }

    // Hash policy
    float load_factor() const {
        return (float) size() / (float) unsafe_bucket_count();
    }

    float max_load_factor() const {
        return my_maximum_bucket_size;
    }

    void max_load_factor(float newmax) {
        if (newmax != newmax || newmax < 0)
            tbb::internal::throw_exception(tbb::internal::eid_invalid_load_factor);
        my_maximum_bucket_size = newmax;
    }

    // This function is a noop, because the underlying split-ordered list
    // is already sorted, so an increase in the bucket number will be
    // reflected next time this bucket is touched.
    void rehash(size_type buckets) {
        size_type current_buckets = my_number_of_buckets;

        if (current_buckets > buckets)
            return;
        else if ( (buckets & (buckets-1)) != 0 )
            tbb::internal::throw_exception(tbb::internal::eid_invalid_buckets_number);
        my_number_of_buckets = buckets;
    }

private:

    // Initialize the hash and keep the first bucket open
    void internal_init() {
        // Allocate an array of segment pointers
        memset(my_buckets, 0, pointers_per_table * sizeof(void *));

        // Insert the first element in the split-ordered list
        raw_iterator dummy_node = my_solist.raw_begin();
        set_bucket(0, dummy_node);
    }

    void internal_clear() {
        for (size_type index = 0; index < pointers_per_table; ++index) {
            if (my_buckets[index] != NULL) {
                size_type sz = segment_size(index);
                for (size_type index2 = 0; index2 < sz; ++index2)
                    my_allocator.destroy(&my_buckets[index][index2]);
                my_allocator.deallocate(my_buckets[index], sz);
                my_buckets[index] = 0;
            }
        }
    }

    void internal_copy(const self_type& right) {
        clear();

        my_maximum_bucket_size = right.my_maximum_bucket_size;
        my_number_of_buckets = right.my_number_of_buckets;

        __TBB_TRY {
            insert(right.begin(), right.end());
            my_hash_compare = right.my_hash_compare;
        } __TBB_CATCH(...) {
            my_solist.clear();
            __TBB_RETHROW();
        }
    }

    void internal_swap_buckets(concurrent_unordered_base& right)
    {
        // Swap all node segments
        for (size_type index = 0; index < pointers_per_table; ++index)
        {
            raw_iterator * iterator_pointer = my_buckets[index];
            my_buckets[index] = right.my_buckets[index];
            right.my_buckets[index] = iterator_pointer;
        }
    }

    // Hash APIs
    size_type internal_distance(const_iterator first, const_iterator last) const
    {
        size_type num = 0;

        for (const_iterator it = first; it != last; ++it)
            ++num;

        return num;
    }

    // Insert an element in the hash given its value
    std::pair<iterator, bool> internal_insert(const value_type& value)
    {
        sokey_t order_key = (sokey_t) my_hash_compare(get_key(value));
        size_type bucket = order_key % my_number_of_buckets;

        // If bucket is empty, initialize it first
        if (!is_initialized(bucket))
            init_bucket(bucket);

        size_type new_count;
        order_key = split_order_key_regular(order_key);
        raw_iterator it = get_bucket(bucket);
        raw_iterator last = my_solist.raw_end();
        raw_iterator where = it;

        __TBB_ASSERT(where != last, "Invalid head node");

        // First node is a dummy node
        ++where;

        for (;;)
        {
            if (where == last || solist_t::get_order_key(where) > order_key)
            {
                // Try to insert it in the right place
                std::pair<iterator, bool> result = my_solist.try_insert(it, where, value, order_key, &new_count);
                
                if (result.second)
                {
                    // Insertion succeeded, adjust the table size, if needed
                    adjust_table_size(new_count, my_number_of_buckets);
                    return result;
                }
                else
                {
                    // Insertion failed: either the same node was inserted by another thread, or
                    // another element was inserted at exactly the same place as this node.
                    // Proceed with the search from the previous location where order key was
                    // known to be larger (note: this is legal only because there is no safe
                    // concurrent erase operation supported).
                    where = it;
                    ++where;
                    continue;
                }
            }
            else if (!allow_multimapping && solist_t::get_order_key(where) == order_key && my_hash_compare(get_key(*where), get_key(value)) == 0)
            {
                // Element already in the list, return it
                return std::pair<iterator, bool>(my_solist.get_iterator(where), false);
            }

            // Move the iterator forward
            it = where;
            ++where;
        }
    }

    // Find the element in the split-ordered list
    iterator internal_find(const key_type& key)
    {
        sokey_t order_key = (sokey_t) my_hash_compare(key);
        size_type bucket = order_key % my_number_of_buckets;

        // If bucket is empty, initialize it first
        if (!is_initialized(bucket))
            init_bucket(bucket);

        order_key = split_order_key_regular(order_key);
        raw_iterator last = my_solist.raw_end();

        for (raw_iterator it = get_bucket(bucket); it != last; ++it)
        {
            if (solist_t::get_order_key(it) > order_key)
            {
                // If the order key is smaller than the current order key, the element
                // is not in the hash.
                return end();
            }
            else if (solist_t::get_order_key(it) == order_key)
            {
                // The fact that order keys match does not mean that the element is found.
                // Key function comparison has to be performed to check whether this is the
                // right element. If not, keep searching while order key is the same.
                if (!my_hash_compare(get_key(*it), key))
                    return my_solist.get_iterator(it);
            }
        }

        return end();
    }

    // Erase an element from the list. This is not a concurrency safe function.
    iterator internal_erase(const_iterator it)
    {
        key_type key = get_key(*it);
        sokey_t order_key = (sokey_t) my_hash_compare(key);
        size_type bucket = order_key % my_number_of_buckets;

        // If bucket is empty, initialize it first
        if (!is_initialized(bucket))
            init_bucket(bucket);

        order_key = split_order_key_regular(order_key);

        raw_iterator previous = get_bucket(bucket);
        raw_iterator last = my_solist.raw_end();
        raw_iterator where = previous;

        __TBB_ASSERT(where != last, "Invalid head node");

        // First node is a dummy node
        ++where;

        for (;;) {
            if (where == last)
                return end();
            else if (my_solist.get_iterator(where) == it)
                return my_solist.erase_node(previous, it);

            // Move the iterator forward
            previous = where;
            ++where;
        }
    }

    // Return the [begin, end) pair of iterators with the same key values.
    // This operation makes sense only if mapping is many-to-one.
    pairii_t internal_equal_range(const key_type& key)
    {
        sokey_t order_key = (sokey_t) my_hash_compare(key);
        size_type bucket = order_key % my_number_of_buckets;

        // If bucket is empty, initialize it first
        if (!is_initialized(bucket))
            init_bucket(bucket);

        order_key = split_order_key_regular(order_key);
        raw_iterator end_it = my_solist.raw_end();

        for (raw_iterator it = get_bucket(bucket); it != end_it; ++it)
        {
            if (solist_t::get_order_key(it) > order_key)
            {
                // There is no element with the given key
                return pairii_t(end(), end());
            }
            else if (solist_t::get_order_key(it) == order_key && !my_hash_compare(get_key(*it), key))
            {
                iterator first = my_solist.get_iterator(it);
                iterator last = first;

                while( last != end() && !my_hash_compare(get_key(*last), key) )
                    ++last;
                return pairii_t(first, last);
            }
        }

        return pairii_t(end(), end());
    }

    // Return the [begin, end) pair of const iterators with the same key values.
    // This operation makes sense only if mapping is many-to-one.
    paircc_t internal_equal_range(const key_type& key) const
    {
        sokey_t order_key = (sokey_t) my_hash_compare(key);
        size_type bucket = order_key % my_number_of_buckets;

        // If bucket is empty, initialize it first
        if (!is_initialized(bucket))
            return paircc_t(end(), end());

        order_key = split_order_key_regular(order_key);
        raw_const_iterator end_it = my_solist.raw_end();

        for (raw_const_iterator it = get_bucket(bucket); it != end_it; ++it)
        {
            if (solist_t::get_order_key(it) > order_key)
            {
                // There is no element with the given key
                return paircc_t(end(), end());
            }
            else if (solist_t::get_order_key(it) == order_key && !my_hash_compare(get_key(*it), key))
            {
                const_iterator first = my_solist.get_iterator(it);
                const_iterator last = first;

                while( last != end() && !my_hash_compare(get_key(*last), key ) )
                    ++last;
                return paircc_t(first, last);
            }
        }

        return paircc_t(end(), end());
    }

    // Bucket APIs
    void init_bucket(size_type bucket)
    {
        // Bucket 0 has no parent. Initialize it and return.
        if (bucket == 0) {
            internal_init();
            return;
        }

        size_type parent_bucket = get_parent(bucket);

        // All parent_bucket buckets have to be initialized before this bucket is
        if (!is_initialized(parent_bucket))
            init_bucket(parent_bucket);

        raw_iterator parent = get_bucket(parent_bucket);

        // Create a dummy first node in this bucket
        raw_iterator dummy_node = my_solist.insert_dummy(parent, split_order_key_dummy(bucket));
        set_bucket(bucket, dummy_node);
    }

    void adjust_table_size(size_type total_elements, size_type current_size)
    {
        // Grow the table by a factor of 2 if possible and needed
        if ( ((float) total_elements / (float) current_size) > my_maximum_bucket_size )
        {
             // Double the size of the hash only if size has not changed inbetween loads
            __TBB_CompareAndSwapW((uintptr_t*)&my_number_of_buckets, 2 * current_size, current_size );
        }
    }

    size_type get_parent(size_type bucket) const
    {
        // Unsets bucket's most significant turned-on bit
        size_type msb = __TBB_Log2((uintptr_t)bucket);
        return bucket & ~(size_type(1) << msb);
    }


    // Dynamic sized array (segments)
    //! @return segment index of given index in the array
    static size_type segment_index_of( size_type index ) {
        return size_type( __TBB_Log2( index|1 ) );
    }

    //! @return the first array index of given segment
    static size_type segment_base( size_type k ) {
        return (size_type(1)<<k & ~size_type(1));
    }

    //! @return segment size
    static size_type segment_size( size_type k ) {
        return k? size_type(1)<<k : 2;
    }

    raw_iterator get_bucket(size_type bucket) const {
        size_type segment = segment_index_of(bucket);
        bucket -= segment_base(segment);
        __TBB_ASSERT( my_buckets[segment], "bucket must be in an allocated segment" );
        return my_buckets[segment][bucket];
    }

    void set_bucket(size_type bucket, raw_iterator dummy_head) {
        size_type segment = segment_index_of(bucket);
        bucket -= segment_base(segment);

        if (my_buckets[segment] == NULL) {
            size_type sz = segment_size(segment);
            raw_iterator * new_segment = my_allocator.allocate(sz);
            std::memset(new_segment, 0, sz*sizeof(raw_iterator));

            if (__TBB_CompareAndSwapW((void *) &my_buckets[segment], (uintptr_t)new_segment, 0) != 0)
                my_allocator.deallocate(new_segment, sz);
        }

        my_buckets[segment][bucket] = dummy_head;
    }

    bool is_initialized(size_type bucket) const {
        size_type segment = segment_index_of(bucket);
        bucket -= segment_base(segment);

        if (my_buckets[segment] == NULL)
            return false;

        raw_iterator it = my_buckets[segment][bucket];
        return (it.get_node_ptr() != NULL);
    }

    // Utilities for keys

    // A regular order key has its original hash value reversed and the last bit set
    sokey_t split_order_key_regular(sokey_t order_key) const {
        return __TBB_ReverseBits(order_key) | 0x1;
    }

    // A dummy order key has its original hash value reversed and the last bit unset
    sokey_t split_order_key_dummy(sokey_t order_key) const {
        return __TBB_ReverseBits(order_key) & ~(0x1);
    }

    // Shared variables
    size_type                                                     my_number_of_buckets;       // Current table size
    solist_t                                                      my_solist;                  // List where all the elements are kept
    typename allocator_type::template rebind<raw_iterator>::other my_allocator;               // Allocator object for segments
    float                                                         my_maximum_bucket_size;     // Maximum size of the bucket
    raw_iterator                                                 *my_buckets[pointers_per_table]; // The segment table
};
#if _MSC_VER
#pragma warning(pop) // warning 4127 -- while (true) has a constant expression in it
#endif

//! Hash multiplier
static const size_t hash_multiplier = sizeof(size_t)==4? 2654435769U : 11400714819323198485ULL;
} // namespace internal
//! @endcond
//! Hasher functions
template<typename T>
inline size_t tbb_hasher( const T& t ) {
    return static_cast<size_t>( t ) * internal::hash_multiplier;
}
template<typename P>
inline size_t tbb_hasher( P* ptr ) {
    size_t const h = reinterpret_cast<size_t>( ptr );
    return (h >> 3) ^ h;
}
template<typename E, typename S, typename A>
inline size_t tbb_hasher( const std::basic_string<E,S,A>& s ) {
    size_t h = 0;
    for( const E* c = s.c_str(); *c; ++c )
        h = static_cast<size_t>(*c) ^ (h * internal::hash_multiplier);
    return h;
}
template<typename F, typename S>
inline size_t tbb_hasher( const std::pair<F,S>& p ) {
    return tbb_hasher(p.first) ^ tbb_hasher(p.second);
}
} // namespace interface5
using interface5::tbb_hasher;
} // namespace tbb
#endif// __TBB_concurrent_unordered_internal_H
