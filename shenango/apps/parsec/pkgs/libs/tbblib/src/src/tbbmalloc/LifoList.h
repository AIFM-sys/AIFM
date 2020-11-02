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

#ifndef _itt_common_malloc_LifoList_H_
#define _itt_common_malloc_LifoList_H_

#include "TypeDefinitions.h"
#include <string.h> // for memset()

//! Checking the synchronization method
/** FINE_GRAIN_LOCKS is the only variant for now; should be defined for LifoList */
#ifndef FINE_GRAIN_LOCKS
#define FINE_GRAIN_LOCKS
#endif

namespace rml {

namespace internal {

class LifoList {
public:
    inline LifoList();
    inline void push(void** ptr);
    inline void* pop(void);
    inline void pushList(void **head, void **tail);

private:
    void * top;
#ifdef FINE_GRAIN_LOCKS
    MallocMutex lock;
#endif /* FINE_GRAIN_LOCKS     */
};

#ifdef FINE_GRAIN_LOCKS
/* LifoList assumes zero initialization so a vector of it can be created
 * by just allocating some space with no call to constructor.
 * On Linux, it seems to be necessary to avoid linking with C++ libraries.
 *
 * By usage convention there is no race on the initialization. */
LifoList::LifoList( ) : top(NULL)
{
    // MallocMutex assumes zero initialization
    memset(&lock, 0, sizeof(MallocMutex));
}

void LifoList::push( void **ptr )
{   
    MallocMutex::scoped_lock scoped_cs(lock);
    *ptr = top;
    top = ptr;
}

void LifoList::pushList( void **head, void **tail )
{   
    MallocMutex::scoped_lock scoped_cs(lock);
    *tail = top;
    top = head;
}

void * LifoList::pop( )
{   
    void **result=NULL;
    if (!top) goto done;
    {
        MallocMutex::scoped_lock scoped_cs(lock);
        if (!top) goto done;
        result = (void **) top;
        top = *result;
    } 
    *result = NULL;
done:
    return result;
}

#endif /* FINE_GRAIN_LOCKS     */

} // namespace internal
} // namespace rml

#endif /* _itt_common_malloc_LifoList_H_ */

