/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-16 Miranda NG project (http://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
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

extern "C"
{
	MIR_CORE_DLL(int) Langpack_MarkPluginLoaded(PLUGININFOEX* pInfo);
	MIR_CORE_DLL(MUUID*) Langpack_LookupUuid(WPARAM wParam);
};

void UnloadLangPackModule(void);

int  InitialiseModularEngine(void);
void DestroyModularEngine(void);

int  InitPathUtils(void);

HINSTANCE ProtoGetInstance(const char *szModuleName);

extern HINSTANCE g_hInst;
extern HWND hAPCWindow;
extern HANDLE hStackMutex, hThreadQueueEmpty;
extern MIDatabase *currDb;

/////////////////////////////////////////////////////////////////////////////////////////
// modules.cpp

struct THookSubscriber
{
	HINSTANCE hOwner;
	int type;
	union {
		struct {
			union {
				MIRANDAHOOK pfnHook;
				MIRANDAHOOKPARAM pfnHookParam;
				MIRANDAHOOKOBJ pfnHookObj;
				MIRANDAHOOKOBJPARAM pfnHookObjParam;
			};
			void* object;
			LPARAM lParam;
		};
		struct {
			HWND hwnd;
			UINT message;
		};
	};
};

#define HOOK_SECRET_SIGNATURE 0xDEADBABA

struct THook
{
	char name[ MAXMODULELABELLENGTH ];
	int  id;
	int  subscriberCount;
	THookSubscriber* subscriber;
	MIRANDAHOOK pfnHook;
	DWORD secretSignature;
	CRITICAL_SECTION csHook;
};

extern LIST<HINSTANCE__> pluginListAddr;

/////////////////////////////////////////////////////////////////////////////////////////
// langpack.cpp

char*  LangPackTranslateString(MUUID* pUuid, const char *szEnglish, const int W);
TCHAR* LangPackTranslateStringT(int hLangpack, const TCHAR* tszEnglish);

/////////////////////////////////////////////////////////////////////////////////////////
// threads.cpp

extern DWORD mir_tls;

/////////////////////////////////////////////////////////////////////////////////////////
// utils.cpp

typedef BOOL(APIENTRY *PGENRANDOM)(PVOID, ULONG);
extern PGENRANDOM pfnRtlGenRandom;
