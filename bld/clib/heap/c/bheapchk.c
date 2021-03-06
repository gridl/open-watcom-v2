/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  perform consistency check on a based heap
*               (16-bit code only)
*
****************************************************************************/


#include "dll.h"        // needs to be first
#include "variety.h"
#include <stddef.h>
#include <malloc.h>
#include "heap.h"
#include "heapacc.h"


static int checkFreeList( unsigned long *free_size, __segment req_seg )
{
    __segment           seg;
    FRLPTR( seg )       frl;
    unsigned long       total_size;

    total_size = 0;
    for( seg = (req_seg == _NULLSEG ? __bheapbeg : req_seg); seg != _NULLSEG; seg = BHEAP( seg )->next.s.segm ) {
        frl = BHEAP( seg )->freehead.next.nptr;
        while( (unsigned)frl != offsetof( heapblk, freehead ) ) {
            total_size += frl->len;
            frl = frl->next.nptr;
        }
        if( req_seg != _NULLSEG ) {
            break;
        }
    }
    *free_size = total_size;
    return( _HEAPOK );
}

static int checkFree( freelist_fptr frl )
{
    __segment           seg;
    FRLPTR( seg )       prev;
    FRLPTR( seg )       next;

    seg = FP_SEG( frl );
    prev = frl->prev.nptr;
    next = frl->next.nptr;
    if( prev->next.nptr != (FRLPTR( seg ))frl || next->prev.nptr != (FRLPTR( seg ))frl ) {
        return( _HEAPBADNODE );
    }
    if( ((FRLPTR( seg ))prev->prev.nptr)->next.nptr != prev || ((FRLPTR( seg ))next->next.nptr)->prev.nptr != next ) {
        return( _HEAPBADNODE );
    }
    return( _HEAPOK );
}

_WCRTLINK int _bheapchk( __segment seg )
{
    struct _heapinfo    hi;
    int                 heap_status;
    unsigned long       free_size;

    if( seg == _DGroup() )
        return( _nheapchk() );
    _AccessFHeap();
    heap_status = checkFreeList( &free_size, seg );
    if( heap_status == _HEAPOK ) {
        hi._pentry = NULL;
        while( (heap_status = __HeapWalk( &hi, (seg == _NULLSEG ? __bheapbeg : seg), seg )) == _HEAPOK ) {
            if( hi._useflag == _FREEENTRY ) {
                heap_status = checkFree( hi._pentry );
                if( heap_status != _HEAPOK )
                    break;
                free_size -= hi._size;
            }
        }
        if( free_size != 0 ) {
            heap_status = _HEAPBADNODE;
        } else if( heap_status == _HEAPBADPTR ) {
            heap_status = _HEAPBADNODE;
        } else if( heap_status == _HEAPEND ) {
            heap_status = _HEAPOK;
        }
    }
    _ReleaseFHeap();
    return( heap_status );
}
