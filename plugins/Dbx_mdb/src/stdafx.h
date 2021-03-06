/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-16 Miranda NG project (http://miranda-ng.org)
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <windows.h>
#include <time.h>
#include <process.h>
#include <memory>
#include <vector>

#include <newpluginapi.h>
#include <win2k.h>
#include <m_system_cpp.h>
#include <m_database.h>
#include <m_langpack.h>
#include <m_clist.h>
#include <m_icolib.h>
#include <m_options.h>
#include <m_crypto.h>
#include <m_metacontacts.h>
#include <m_protocols.h>
#include <m_netlib.h>
#include <m_gui.h>

#include "lmdb\lmdb.h"

#ifndef thread_local
#	define thread_local __declspec(thread)
#endif


class txn_ptr
{
	MDB_txn *m_txn;

public:
	__forceinline txn_ptr(MDB_env *pEnv)
	{
		mdb_txn_begin(pEnv, NULL, 0, &m_txn);
	}

	__forceinline ~txn_ptr()
	{
		if (m_txn)
			mdb_txn_abort(m_txn);
	}

	__forceinline operator MDB_txn*() const { return m_txn; }

	__forceinline bool commit()
	{
		bool bRes = (mdb_txn_commit(m_txn) != MDB_MAP_FULL);
		m_txn = NULL;
		return bRes;
	}

	__forceinline void abort()
	{
		mdb_txn_abort(m_txn);
		m_txn = NULL;
	}
};

class txn_ptr_ro
{
	MDB_txn *m_txn;
public:
	__forceinline txn_ptr_ro(MDB_txn *txn) : m_txn(txn)
	{
		mdb_txn_renew(m_txn);
	}
	__forceinline ~txn_ptr_ro()
	{
		mdb_txn_reset(m_txn);
	}
	__forceinline operator MDB_txn*() const { return m_txn; }
};

class cursor_ptr
{
	MDB_cursor *m_cursor;

public:
	__forceinline cursor_ptr(MDB_txn *_txn, MDB_dbi _dbi)
	{
		if (mdb_cursor_open(_txn, _dbi, &m_cursor) != MDB_SUCCESS)
			m_cursor = NULL;
	}

	__forceinline ~cursor_ptr()
	{
		if (m_cursor)
			mdb_cursor_close(m_cursor);
	}

	__forceinline operator MDB_cursor*() const { return m_cursor; }
};

class cursor_ptr_ro
{
	MDB_cursor *m_cursor;
public:
	__forceinline cursor_ptr_ro(MDB_cursor *cursor) : m_cursor(cursor)
	{
		mdb_cursor_renew(mdb_cursor_txn(m_cursor), m_cursor); //--
	}
	__forceinline operator MDB_cursor*() const { return m_cursor; }
};

#define MDB_CHECK(A,B) \
	switch(A) { \
	case MDB_SUCCESS: break; \
	case MDB_MAP_FULL: continue; \
	default: return (B); }

#include "dbintf.h"
#include "resource.h"
#include "version.h"

extern HINSTANCE g_hInst;
extern LIST<CDbxMdb> g_Dbs;

#include "ui.h"
