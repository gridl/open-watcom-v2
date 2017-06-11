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
* Description:  ORL hash table definition.
*
****************************************************************************/


#ifndef ORL_HASH_DEF_INCLUDED
#define ORL_HASH_DEF_INCLUDED

/* hash table definitions */
typedef enum {
    ORL_HASH_STRING,
    ORL_HASH_STRING_IGNORECASE,
    ORL_HASH_NUMBER
} orl_hash_table_type;

typedef unsigned    orl_hash_value;
typedef const void  *orl_hash_data;
typedef struct {
    union {
        pointer_int     number;
        const char      *string;
    } u;
} orl_hash_key;

typedef struct orl_hash_data_struct     orl_hash_data_struct;
typedef struct orl_hash_entry_struct    orl_hash_entry_struct;
typedef struct orl_hash_table_struct    orl_hash_table_struct;

struct orl_hash_data_struct {
    orl_hash_data               data;
    orl_hash_data_struct        *next;
};

struct orl_hash_entry_struct {
    orl_hash_key                key;
    orl_hash_data_struct        *data_entry;
    orl_hash_entry_struct       *next;
};

struct orl_hash_table_struct {
    orl_hash_value              size;
    orl_funcs                   *funcs;
    bool                        (*compare_func)( orl_hash_key, orl_hash_key );
    orl_hash_value              (*hash_func)( orl_hash_value, orl_hash_key );
    orl_hash_entry_struct       **table;
};

typedef orl_hash_table_struct   *orl_hash_table;

#endif
