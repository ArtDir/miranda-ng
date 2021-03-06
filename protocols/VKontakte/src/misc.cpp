/*
Copyright (c) 2013-16 Miranda NG project (http://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

static const char *szImageTypes[] = { "photo_2560", "photo_1280", "photo_807", "photo_604", "photo_256", "photo_130", "photo_128", "photo_75", "photo_64" , "preview"};

static const char  *szGiftTypes[] = { "thumb_256", "thumb_96", "thumb_48" };

JSONNode nullNode(JSON_NULL);

bool IsEmpty(LPCTSTR str)
{
	return (str == NULL || str[0] == 0);
}

bool IsEmpty(LPCSTR str)
{
	return (str == NULL || str[0] == 0);
}

LPCSTR findHeader(NETLIBHTTPREQUEST *pReq, LPCSTR szField)
{
	for (int i = 0; i < pReq->headersCount; i++)
		if (!_stricmp(pReq->headers[i].szName, szField))
			return pReq->headers[i].szValue;

	return NULL;
}

bool tlstrstr(TCHAR *_s1, TCHAR *_s2)
{
	TCHAR s1[1024], s2[1024];

	_tcsncpy_s(s1, _s1, _TRUNCATE);
	CharLowerBuff(s1, _countof(s1));
	_tcsncpy_s(s2, _s2, _TRUNCATE);
	CharLowerBuff(s2, _countof(s2));

	return _tcsstr(s1, s2) != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////

static IconItem iconList[] =
{
	{ LPGEN("Captcha form icon"), "key", IDI_KEYS },
	{ LPGEN("Notification icon"), "notification", IDI_NOTIFICATION },
	{ LPGEN("Read message icon"), "read", IDI_READMSG },
	{ LPGEN("Visit profile icon"), "profile", IDI_VISITPROFILE },
	{ LPGEN("Load server history icon"), "history", IDI_HISTORY },
	{ LPGEN("Add to friend list icon"), "addfriend", IDI_FRIENDADD },
	{ LPGEN("Delete from friend list icon"), "delfriend", IDI_FRIENDDEL },
	{ LPGEN("Report abuse icon"), "abuse", IDI_ABUSE },
	{ LPGEN("Ban user icon"), "ban", IDI_BAN },
	{ LPGEN("Broadcast icon"), "broadcast", IDI_BROADCAST },
	{ LPGEN("Status icon"), "status", IDI_STATUS },
	{ LPGEN("Wall message icon"), "wall", IDI_WALL },
	{ LPGEN("Mark messages as read icon"), "markread", IDI_MARKMESSAGESASREAD }
};

void InitIcons()
{
	Icon_Register(hInst, LPGEN("Protocols") "/" LPGEN("VKontakte"), iconList, _countof(iconList), "VKontakte");
}

HANDLE GetIconHandle(int iCommand)
{
	for (int i = 0; i < _countof(iconList); i++)
		if (iconList[i].defIconID == iCommand)
			return iconList[i].hIcolib;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

char* ExpUrlEncode(const char *szUrl, bool strict)
{
	const char szHexDigits[] = "0123456789ABCDEF";

	if (szUrl == NULL)
		return NULL;

	const BYTE *s;
	int outputLen;
	for (outputLen = 0, s = (const BYTE*)szUrl; *s; s++) 
		if ((*s & 0x80 && !strict) || // UTF-8 multibyte
			('0' <= *s && *s <= '9') || //0-9
			('A' <= *s && *s <= 'Z') || //ABC...XYZ
			('a' <= *s && *s <= 'z') || //abc...xyz
			*s == '~' || *s == '-' || *s == '_' 	|| *s == '.' || *s == ' ') 
			outputLen++;
		else 
			outputLen += 3;

	char *szOutput = (char*)mir_alloc(outputLen + 1);
	if (szOutput == NULL)
		return NULL;

	char *d = szOutput;
	for (s = (const BYTE*)szUrl; *s; s++)
		if ((*s & 0x80 && !strict) || // UTF-8 multibyte
			('0' <= *s && *s <= '9') || //0-9
			('A' <= *s && *s <= 'Z') || //ABC...XYZ
			('a' <= *s && *s <= 'z') || //abc...xyz
			*s == '~' || *s == '-' || *s == '_' || *s == '.') 
			*d++ = *s;
		else if (*s == ' ') 
			*d++ = '+';
		else {
			*d++ = '%';
			*d++ = szHexDigits[*s >> 4];
			*d++ = szHexDigits[*s & 0xF];
		}

	*d = '\0';
	return szOutput;
}


/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::ClearAccessToken()
{
	debugLogA("CVkProto::ClearAccessToken");
	setDword("LastAccessTokenTime", (DWORD)time(NULL));
	m_szAccessToken = NULL;
	delSetting("AccessToken");
	ShutdownSession();
}

TCHAR* CVkProto::GetUserStoredPassword()
{
	debugLogA("CVkProto::GetUserStoredPassword");
	ptrA szRawPass(getStringA("Password"));
	return (szRawPass != NULL) ? mir_utf8decodeT(szRawPass) : NULL;
}

void CVkProto::SetAllContactStatuses(int iStatus)
{
	debugLogA("CVkProto::SetAllContactStatuses (%d)", iStatus);
	for (MCONTACT hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName)) {
		if (isChatRoom(hContact))
			SetChatStatus(hContact, iStatus);
		else if (getWord(hContact, "Status") != iStatus)
			setWord(hContact, "Status", iStatus);

		if (iStatus == ID_STATUS_OFFLINE) {
			SetMirVer(hContact, -1);
			db_unset(hContact, m_szModuleName, "ListeningTo");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

MCONTACT CVkProto::FindUser(LONG dwUserid, bool bCreate)
{
	if (!dwUserid)
		return NULL;

	for (MCONTACT hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName)) {
		LONG dbUserid = getDword(hContact, "ID", -1);
		if (dbUserid == -1)
			continue;

		if (dbUserid == dwUserid)
			return hContact;
	}

	if (!bCreate)
		return NULL;

	MCONTACT hNewContact = (MCONTACT)CallService(MS_DB_CONTACT_ADD);
	Proto_AddToContact(hNewContact, m_szModuleName);
	setDword(hNewContact, "ID", dwUserid);
	db_set_ts(hNewContact, "CList", "Group", m_vkOptions.ptszDefaultGroup);
	return hNewContact;
}

MCONTACT CVkProto::FindChat(LONG dwUserid)
{
	if (!dwUserid)
		return NULL;

	for (MCONTACT hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName)) {
		LONG dbUserid = getDword(hContact, "vk_chat_id", -1);
		if (dbUserid == -1)
			continue;

		if (dbUserid == dwUserid)
			return hContact;
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool CVkProto::CheckMid(LIST<void> &lList, int guid)
{
	for (int i = lList.getCount() - 1; i >= 0; i--)
		if ((INT_PTR)lList[i] == guid) {
			lList.remove(i);
			return true;
		}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////

JSONNode& CVkProto::CheckJsonResponse(AsyncHttpRequest *pReq, NETLIBHTTPREQUEST *reply, JSONNode &root)
{
	debugLogA("CVkProto::CheckJsonResponse");

	if (!reply || !reply->pData)
		return nullNode;

	root = JSONNode::parse(reply->pData);

	if (!CheckJsonResult(pReq, root))
		return nullNode;

	return root["response"];
}

bool CVkProto::CheckJsonResult(AsyncHttpRequest *pReq, const JSONNode &jnNode)
{
	debugLogA("CVkProto::CheckJsonResult");
	if (!jnNode) {
		pReq->m_iErrorCode = VKERR_NO_JSONNODE;
		return false;
	}

	const JSONNode &jnError = jnNode["error"];
	const JSONNode &jnErrorCode = jnError["error_code"];

	if (!jnError || !jnErrorCode)
		return true;

	pReq->m_iErrorCode = jnErrorCode.as_int();
	debugLogA("CVkProto::CheckJsonResult %d", pReq->m_iErrorCode);
	switch (pReq->m_iErrorCode) {
	case VKERR_AUTHORIZATION_FAILED:
		ConnectionFailed(LOGINERR_WRONGPASSWORD);
		break;
	case VKERR_ACCESS_DENIED:
		if (time(NULL) - getDword("LastAccessTokenTime", 0) > 60 * 60 * 24) {
			debugLogA("CVkProto::CheckJsonResult VKERR_ACCESS_DENIED (AccessToken fail?)");
			ClearAccessToken();
			return false;
		}
		debugLogA("CVkProto::CheckJsonResult VKERR_ACCESS_DENIED");
		MsgPopup(NULL, TranslateT("Access denied! Data will not be sent or received."), TranslateT("Error"), true);
		break;
	case VKERR_CAPTCHA_NEEDED:
		ApplyCaptcha(pReq, jnError);
		break;
	case VKERR_VALIDATION_REQUIRED:	// Validation Required
		MsgPopup(NULL, TranslateT("You must validate your account before use VK in Miranda NG"), TranslateT("Error"), true);
		break;
	case VKERR_FLOOD_CONTROL:
		pReq->m_iRetry = 0;
		// fall through
	case VKERR_UNKNOWN:
	case VKERR_TOO_MANY_REQ_PER_SEC:
	case VKERR_INTERNAL_SERVER_ERR:
		if (pReq->m_iRetry > 0) {
			pReq->bNeedsRestart = true;
			Sleep(500); //Pause for fix err 
			debugLogA("CVkProto::CheckJsonResult Retry = %d", pReq->m_iRetry);
			pReq->m_iRetry--;
		}
		else {
			CMString msg(FORMAT, TranslateT("Error %d. Data will not be sent or received."), pReq->m_iErrorCode);
			MsgPopup(NULL, msg, TranslateT("Error"), true);
			debugLogA("CVkProto::CheckJsonResult SendError");
		}
		break;

	case VKERR_INVALID_PARAMETERS:
		MsgPopup(NULL, TranslateT("One of the parameters specified was missing or invalid"), TranslateT("Error"), true);
		break;
	case VKERR_ACC_WALL_POST_DENIED:
		MsgPopup(NULL, TranslateT("Access to adding post denied"), TranslateT("Error"), true);
		break;
	case VKERR_COULD_NOT_SAVE_FILE:
	case VKERR_INVALID_ALBUM_ID:
	case VKERR_INVALID_SERVER:
	case VKERR_INVALID_HASH:
	case VKERR_INVALID_AUDIO:
	case VKERR_AUDIO_DEL_COPYRIGHT:
	case VKERR_INVALID_FILENAME:
	case VKERR_INVALID_FILESIZE:
	case VKERR_HIMSELF_AS_FRIEND:
	case VKERR_YOU_ON_BLACKLIST:
	case VKERR_USER_ON_BLACKLIST:
		// See CVkProto::SendFileFiled 
		break;
	}

	return pReq->m_iErrorCode == 0;
}

void CVkProto::OnReceiveSmth(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	JSONNode jnRoot;
	const JSONNode &jnResponse = CheckJsonResponse(pReq, reply, jnRoot);
	debugLogA("CVkProto::OnReceiveSmth %d", jnResponse.as_int());
}

/////////////////////////////////////////////////////////////////////////////////////////
// Quick & dirty form parser

static CMStringA getAttr(char *szSrc, LPCSTR szAttrName)
{
	char *pEnd = strchr(szSrc, '>');
	if (pEnd == NULL)
		return "";

	*pEnd = 0;

	char *p1 = strstr(szSrc, szAttrName);
	if (p1 == NULL) {
		*pEnd = '>';
		return "";
	}

	p1 += mir_strlen(szAttrName);
	if (p1[0] != '=' || p1[1] != '\"') {
		*pEnd = '>';
		return "";
	}

	p1 += 2;
	char *p2 = strchr(p1, '\"');
	*pEnd = '>';
	if (p2 == NULL) 
		return "";

	return CMStringA(p1, (int)(p2-p1));
}

bool CVkProto::AutoFillForm(char *pBody, CMStringA &szAction, CMStringA& szResult)
{
	debugLogA("CVkProto::AutoFillForm");
	szResult.Empty();

	char *pFormBeg = strstr(pBody, "<form ");
	if (pFormBeg == NULL) 
		return false;

	char *pFormEnd = strstr(pFormBeg, "</form>");
	if (pFormEnd == NULL) 
		return false;

	*pFormEnd = 0;

	szAction = getAttr(pFormBeg, "action");

	CMStringA result;
	char *pFieldBeg = pFormBeg;
	while (true) {
		if ((pFieldBeg = strstr(pFieldBeg+1, "<input ")) == NULL)
			break;

		CMStringA type = getAttr(pFieldBeg, "type");
		if (type != "submit") {
			CMStringA name = getAttr(pFieldBeg, "name");
			CMStringA value = getAttr(pFieldBeg, "value");
			if (name == "email")
				value = (char*)T2Utf(ptrT(getTStringA("Login")));
			else if (name == "pass")
				value = (char*)T2Utf(ptrT(GetUserStoredPassword()));
			else if (name == "captcha_key") {
				char *pCaptchaBeg = strstr(pFormBeg, "<img id=\"captcha\"");
				if (pCaptchaBeg != NULL)
					if (!RunCaptchaForm(getAttr(pCaptchaBeg, "src"), value))
						return false;
			}
			else if (name == "code") 
				value = RunConfirmationCode();

			if (!result.IsEmpty())
				result.AppendChar('&');
			result += name + "=";
			result += ptrA(mir_urlEncode(value));
		}
	}

	szResult = result;
	debugLogA("CVkProto::AutoFillForm result = \"%s\"", szResult);
	return true;
}

CMString CVkProto::RunConfirmationCode()
{
	ENTER_STRING pForm = { sizeof(pForm) };
	pForm.type = ESF_PASSWORD;
	pForm.caption = TranslateT("Enter confirmation code");
	pForm.ptszInitVal = NULL;
	pForm.szModuleName = m_szModuleName;
	pForm.szDataPrefix = "confirmcode_";
	return (!EnterString(&pForm)) ? CMString() : CMString(ptrT(pForm.ptszResult));
}

CMString CVkProto::RunRenameNick(LPCTSTR ptszOldName)
{
	ENTER_STRING pForm = { sizeof(pForm) };
	pForm.type = ESF_COMBO;
	pForm.recentCount = 0;
	pForm.caption = TranslateT("Enter new nickname");
	pForm.ptszInitVal = ptszOldName;
	pForm.szModuleName = m_szModuleName;
	pForm.szDataPrefix = "renamenick_";
	return (!EnterString(&pForm)) ? CMString() : CMString(ptrT(pForm.ptszResult));
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::GrabCookies(NETLIBHTTPREQUEST *nhr)
{
	debugLogA("CVkProto::GrabCookies");
	for (int i = 0; i < nhr->headersCount; i++) {
		if (_stricmp(nhr->headers[i].szName, "Set-cookie"))
			continue;

		CMStringA szValue = nhr->headers[i].szValue, szCookieName, szCookieVal, szDomain;
		int iStart = 0;
		while (true) {
			bool bFirstToken = (iStart == 0);
			CMStringA szToken = szValue.Tokenize(";", iStart).Trim();
			if (iStart == -1)
				break;

			if (bFirstToken) {
				int iStart2 = 0;
				szCookieName = szToken.Tokenize("=", iStart2);
				szCookieVal = szToken.Tokenize("=", iStart2);
			}
			else if (!strncmp(szToken, "domain=", 7))
				szDomain = szToken.Mid(7);
		}

		if (!szCookieName.IsEmpty() && !szDomain.IsEmpty()) {
			int k;
			for (k=0; k < m_cookies.getCount(); k++) {
				if (m_cookies[k].m_name == szCookieName) {
					m_cookies[k].m_value = szCookieVal;
					break;
				}
			}
			if (k == m_cookies.getCount())
				m_cookies.insert(new CVkCookie(szCookieName, szCookieVal, szDomain));
		}
	}
}

void CVkProto::ApplyCookies(AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::ApplyCookies");
	CMStringA szCookie;

	for (int i=0; i < m_cookies.getCount(); i++) {
		if (!strstr(pReq->m_szUrl, m_cookies[i].m_domain))
			continue;

		if (!szCookie.IsEmpty())
			szCookie.Append("; ");
		szCookie.Append(m_cookies[i].m_name);
		szCookie.AppendChar('=');
		szCookie.Append(m_cookies[i].m_value);
	}

	if (!szCookie.IsEmpty())
		pReq->AddHeader("Cookie", szCookie);
}

/////////////////////////////////////////////////////////////////////////////////////////

void __cdecl CVkProto::DBAddAuthRequestThread(void *p)
{
	MCONTACT hContact = (UINT_PTR)p;
	if (hContact == NULL || hContact == INVALID_CONTACT_ID || !IsOnline())
		return;

	for (int i = 0; i < MAX_RETRIES && IsEmpty(ptrT(db_get_tsa(hContact, m_szModuleName, "Nick"))); i++) {
		Sleep(1500);
		
		if (!IsOnline())
			return;
	}

	DBAddAuthRequest(hContact);
}

void CVkProto::DBAddAuthRequest(const MCONTACT hContact)
{
	debugLogA("CVkProto::DBAddAuthRequest");

	T2Utf szNick(ptrT(db_get_tsa(hContact, m_szModuleName, "Nick")));
	T2Utf szFirstName(ptrT(db_get_tsa(hContact, m_szModuleName, "FirstName")));
	T2Utf szLastName(ptrT(db_get_tsa(hContact, m_szModuleName, "LastName")));

	//blob is: uin(DWORD), hContact(DWORD), nick(ASCIIZ), first(ASCIIZ), last(ASCIIZ), email(ASCIIZ), reason(ASCIIZ)
	//blob is: 0(DWORD), hContact(DWORD), nick(ASCIIZ), first(ASCIIZ), last(ASCIIZ), ""(ASCIIZ), ""(ASCIIZ)
	DBEVENTINFO dbei = { sizeof(DBEVENTINFO) };
	dbei.szModule = m_szModuleName;
	dbei.timestamp = (DWORD)time(NULL);
	dbei.flags = DBEF_UTF;
	dbei.eventType = EVENTTYPE_AUTHREQUEST;
	dbei.cbBlob = (DWORD)(sizeof(DWORD) * 2 + mir_strlen(szNick) + mir_strlen(szFirstName) + mir_strlen(szLastName) + 5);

	PBYTE pCurBlob = dbei.pBlob = (PBYTE)mir_alloc(dbei.cbBlob);

	*((PDWORD)pCurBlob) = 0; 
	pCurBlob += sizeof(DWORD); // uin(DWORD) = 0 (DWORD)
	
	*((PDWORD)pCurBlob) = (DWORD)hContact;
	pCurBlob += sizeof(DWORD); // hContact(DWORD)

	mir_strcpy((char*)pCurBlob, szNick); 
	pCurBlob += mir_strlen(szNick) + 1;

	mir_strcpy((char*)pCurBlob, szFirstName);
	pCurBlob += mir_strlen(szFirstName) + 1;

	mir_strcpy((char*)pCurBlob, szLastName);
	pCurBlob += mir_strlen(szLastName) + 1;
	
	*pCurBlob = '\0';	//email
	pCurBlob++;
	*pCurBlob = '\0';	//reason

	db_event_add(NULL, &dbei);
	debugLogA("CVkProto::DBAddAuthRequest %s", szNick ? szNick : "<null>");
}

MCONTACT CVkProto::MContactFromDbEvent(MEVENT hDbEvent)
{
	debugLogA("CVkProto::MContactFromDbEvent");
	if (!hDbEvent || !IsOnline())
		return INVALID_CONTACT_ID;

	DWORD body[2];
	DBEVENTINFO dbei = { sizeof(dbei) };
	dbei.cbBlob = sizeof(DWORD) * 2;
	dbei.pBlob = (PBYTE)&body;

	if (db_event_get(hDbEvent, &dbei))
		return INVALID_CONTACT_ID;
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST || mir_strcmp(dbei.szModule, m_szModuleName))
		return INVALID_CONTACT_ID;

	MCONTACT hContact = DbGetAuthEventContact(&dbei);
	db_unset(hContact, m_szModuleName, "ReqAuth");
	return hContact;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::SetMirVer(MCONTACT hContact, int platform)
{
	if (hContact == NULL || hContact == INVALID_CONTACT_ID)
		return;
	if (platform == -1) {
		db_unset(hContact, m_szModuleName, "MirVer");
		return;
	}

	CMString MirVer, OldMirVer(ptrT(db_get_tsa(hContact, m_szModuleName, "MirVer")));
	bool bSetFlag = true;

	switch (platform) {
	case VK_APP_ID:
		MirVer = _T("Miranda NG VKontakte");
		break;
	case 2386311:
		MirVer = _T("QIP 2012 VKontakte");
		break;
	case 1:
		MirVer = _T("VKontakte (Mobile)");
		break;
	case 3087106: // iPhone
	case 3140623:
	case 2:
		MirVer = _T("VKontakte (iPhone)");
		break;
	case 3682744: // iPad
	case 3:
		MirVer = _T("VKontakte (iPad)");
		break;
	case 2685278: // Android - Kate
		MirVer = _T("Kate Mobile (Android)");
		break;
	case 2890984: // Android
	case 2274003:
	case 4:
		MirVer = _T("VKontakte (Android)");
		break;
	case 3059453: // Windows Phone
	case 2424737:
	case 3502561:
	case 5:
		MirVer = _T("VKontakte (WPhone)");
		break;
	case 3584591: // Windows 8.x
	case 6:
		MirVer = _T("VKontakte (Windows)");
		break; 
	case 7:
		MirVer = _T("VKontakte (Website)");
		break;
	default:
		MirVer = _T("VKontakte (Other)");
		bSetFlag = OldMirVer.IsEmpty();
	}

	if (OldMirVer == MirVer)
		return;

	if (bSetFlag)
		setTString(hContact, "MirVer", MirVer);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::ContactTypingThread(void *p)
{
	debugLogA("CVkProto::ContactTypingThread");
	MCONTACT hContact = (UINT_PTR)p;
	CallService(MS_PROTO_CONTACTISTYPING, hContact, 5);
	Sleep(4500);
	CallService(MS_PROTO_CONTACTISTYPING, hContact);

	if (!ServiceExists(MS_MESSAGESTATE_UPDATE)) {
		Sleep(1500);
		SetSrmmReadStatus(hContact);
	}
}

int CVkProto::OnProcessSrmmEvent(WPARAM, LPARAM lParam)
{
	debugLogA("CVkProto::OnProcessSrmmEvent");
	MessageWindowEventData *event = (MessageWindowEventData *)lParam;

	if (event->uType == MSG_WINDOW_EVT_OPENING && !ServiceExists(MS_MESSAGESTATE_UPDATE))
		SetSrmmReadStatus(event->hContact);

	return 0;
}

void CVkProto::SetSrmmReadStatus(MCONTACT hContact)
{
	time_t time = getDword(hContact, "LastMsgReadTime");
	if (!time)
		return;

	TCHAR ttime[64];
	_locale_t locale = _create_locale(LC_ALL, "");
	_tcsftime_l(ttime, _countof(ttime), _T("%X - %x"), localtime(&time), locale);
	_free_locale(locale);

	StatusTextData st = { 0 };
	st.cbSize = sizeof(st);
	st.hIcon = IcoLib_GetIconByHandle(GetIconHandle(IDI_READMSG));
	mir_sntprintf(st.tszText, TranslateT("Message read: %s"), ttime);
	CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hContact, (LPARAM)&st);
}

void CVkProto::MarkDialogAsRead(MCONTACT hContact) 
{
	debugLogA("CVkProto::MarkDialogAsRead");
	if (!IsOnline())
		return;

	LONG userID = getDword(hContact, "ID", -1);
	if (userID == -1 || userID == VK_FEED_USER)
		return;

	MEVENT hDBEvent = NULL;
	MCONTACT hMContact = db_mc_tryMeta(hContact);
	while ((hDBEvent = db_event_firstUnread(hContact)) != NULL) 
	{
		DBEVENTINFO dbei = { sizeof(dbei) };
		if (!db_event_get(hDBEvent, &dbei) && !mir_strcmp(m_szModuleName, dbei.szModule))
		{
			db_event_markRead(hContact, hDBEvent);
			pcli->pfnRemoveEvent(hMContact, hDBEvent);
			if (hContact != hMContact)
				pcli->pfnRemoveEvent(hContact, hDBEvent);
		}
	}
}

char* CVkProto::GetStickerId(const char *Msg, int &stickerid)
{
	stickerid = 0;
	char *retMsg = NULL;

	int iRes = 0;
	char HeadMsg[32] = { 0 };
	const char *tmpMsg = strstr(Msg, "[sticker:");
	if (tmpMsg)
		iRes = sscanf(tmpMsg, "[sticker:%d]", &stickerid);
	if (iRes == 1) {
		mir_snprintf(HeadMsg, "[sticker:%d]", stickerid);
		size_t retLen = mir_strlen(HeadMsg);
		if (retLen < mir_strlen(Msg)) {
			CMStringA szMsg(Msg, int(mir_strlen(Msg) - mir_strlen(tmpMsg)));
			szMsg.Append(&tmpMsg[retLen]);
			retMsg = mir_strdup(szMsg.Trim());
		}	
	}

	return retMsg;
}

int CVkProto::OnDbSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;
	if (hContact != NULL)
		return 0;

	if (strcmp(cws->szModule, "ListeningTo"))
		return 0;

	CMStringA szListeningTo(m_szModuleName);
	szListeningTo += "Enabled";
	if (!strcmp(cws->szSetting, szListeningTo)) {
		MusicSendMetod iOldMusicSendMetod = (MusicSendMetod)getByte("OldMusicSendMetod", sendBroadcastAndStatus);
		
		if (cws->value.bVal == 0)
			setByte("OldMusicSendMetod", m_vkOptions.iMusicSendMetod);
		else
			db_unset(0, m_szModuleName, "OldMusicSendMetod");
		
		m_vkOptions.iMusicSendMetod = cws->value.bVal == 0 ? sendNone : iOldMusicSendMetod;
		setByte("MusicSendMetod", m_vkOptions.iMusicSendMetod);
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

CMString CVkProto::SpanVKNotificationType(CMString& tszType, VKObjType& vkFeedback, VKObjType& vkParent)
{
	CVKNotification vkNotification[] = {
		// type, parent, feedback, string for translate
		{ _T("group"), vkInvite, vkNull, TranslateT("has invited you to a group") },
		{ _T("page"), vkInvite, vkNull, TranslateT("has invited you to subscribe to a page") },
		{ _T("event"), vkInvite, vkNull, TranslateT("invites you to event") },

		{ _T("follow"), vkNull, vkUsers, _T("") },
		{ _T("friend_accepted"), vkNull, vkUsers, _T("") },
		{ _T("mention"), vkNull, vkPost, _T("") },
		{ _T("wall"), vkNull, vkPost, _T("") },
		{ _T("wall_publish"), vkNull, vkPost, _T("") },

		{ _T("comment_post"), vkPost, vkComment, TranslateT("commented on your post") },
		{ _T("comment_photo"), vkPhoto, vkComment, TranslateT("commented on your photo") },
		{ _T("comment_video"), vkVideo, vkComment, TranslateT("commented on your video") },
		{ _T("reply_comment"), vkComment, vkComment, TranslateT("replied to your comment") },
		{ _T("reply_comment_photo"), vkComment, vkComment, TranslateT("replied to your comment to photo") },
		{ _T("reply_comment_video"), vkComment, vkComment, TranslateT("replied to your comment to video") },
		{ _T("reply_topic"), vkTopic, vkComment, TranslateT("replied to your topic") },
		{ _T("like_post"), vkPost, vkUsers, TranslateT("liked your post") },
		{ _T("like_comment"), vkComment, vkUsers, TranslateT("liked your comment") },
		{ _T("like_photo"), vkPhoto, vkUsers, TranslateT("liked your photo") },
		{ _T("like_video"), vkVideo, vkUsers, TranslateT("liked your video") },
		{ _T("like_comment_photo"), vkComment, vkUsers, TranslateT("liked your comment to photo") },
		{ _T("like_comment_video"), vkComment, vkUsers, TranslateT("liked your comment to video" ) },
		{ _T("like_comment_topic"), vkComment, vkUsers, TranslateT("liked your comment to topic") },
		{ _T("copy_post"), vkPost, vkCopy, TranslateT("shared your post") },
		{ _T("copy_photo"), vkPhoto, vkCopy, TranslateT("shared your photo") },
		{ _T("copy_video"), vkVideo, vkCopy, TranslateT("shared your video") },
		{ _T("mention_comments"), vkPost, vkComment, _T("mentioned you in comment") },
		{ _T("mention_comment_photo"), vkPhoto, vkComment, _T("mentioned you in comment to photo") }, 
		{ _T("mention_comment_video"), vkVideo, vkComment, _T("mentioned you in comment to video") }
	};

	CMString tszRes;
	vkFeedback = vkParent = vkNull;
	for (int i = 0; i < _countof(vkNotification); i++)
		if (tszType == vkNotification[i].ptszType) {
			vkFeedback = vkNotification[i].vkFeedback;
			vkParent = vkNotification[i].vkParent;
			tszRes = vkNotification[i].ptszTranslate;
			break;
		}
	return tszRes;
}

CMString CVkProto::GetVkPhotoItem(const JSONNode &jnPhoto, BBCSupport iBBC)
{
	CMString tszRes;

	if (!jnPhoto)
		return tszRes;

	CMString tszLink, tszPreviewLink;
	for (int i = 0; i < _countof(szImageTypes); i++) {
		const JSONNode &n = jnPhoto[szImageTypes[i]];
		if (n) {
			tszLink = n.as_mstring();
			break;
		}
	}

	switch (m_vkOptions.iIMGBBCSupport) {
	case imgNo:
		tszPreviewLink = _T("");
		break;
	case imgFullSize:
		tszPreviewLink = tszLink;
		break;
	case imgPreview130:
	case imgPreview604:
		tszPreviewLink = jnPhoto[m_vkOptions.iIMGBBCSupport == imgPreview130 ? "photo_130" : "photo_604"].as_mstring();
		break;
	}

	int iWidth = jnPhoto["width"].as_int();
	int iHeight = jnPhoto["height"].as_int();

	tszRes.AppendFormat(_T("%s (%dx%d)"), SetBBCString(TranslateT("Photo"), iBBC, vkbbcUrl, tszLink), iWidth, iHeight);
	if (m_vkOptions.iIMGBBCSupport && iBBC != bbcNo)
		tszRes.AppendFormat(_T("\n\t%s"), SetBBCString(!tszPreviewLink.IsEmpty() ? tszPreviewLink : (!tszLink.IsEmpty() ? tszLink : _T("")), bbcBasic, vkbbcImg));
	CMString tszText(jnPhoto["text"].as_mstring());
	if (!tszText.IsEmpty())
		tszRes += _T("\n") + tszText;

	return tszRes;
}

CMString CVkProto::SetBBCString(LPCTSTR ptszString, BBCSupport iBBC, VKBBCType bbcType, LPCTSTR tszAddString)
{
	CVKBBCItem bbcItem[] = {
		{ vkbbcB, bbcNo, _T("%s") },
		{ vkbbcB, bbcBasic, _T("[b]%s[/b]") },
		{ vkbbcB, bbcAdvanced, _T("[b]%s[/b]") },
		{ vkbbcI, bbcNo, _T("%s") },
		{ vkbbcI, bbcBasic, _T("[i]%s[/i]") },
		{ vkbbcI, bbcAdvanced, _T("[i]%s[/i]") },
		{ vkbbcS, bbcNo, _T("%s") },
		{ vkbbcS, bbcBasic, _T("[s]%s[/s]") },
		{ vkbbcS, bbcAdvanced, _T("[s]%s[/s]") },
		{ vkbbcU, bbcNo, _T("%s") },
		{ vkbbcU, bbcBasic, _T("[u]%s[/u]") },
		{ vkbbcU, bbcAdvanced, _T("[u]%s[/u]") },
		{ vkbbcCode, bbcNo, _T("%s") },
		{ vkbbcCode, bbcBasic, _T("%s") },
		{ vkbbcCode, bbcAdvanced, _T("[code]%s[/code]") },
		{ vkbbcImg, bbcNo, _T("%s") },
		{ vkbbcImg, bbcBasic, _T("[img]%s[/img]") },
		{ vkbbcImg, bbcAdvanced, _T("[img]%s[/img]") },
		{ vkbbcUrl, bbcNo, _T("%s (%s)") },
		{ vkbbcUrl, bbcBasic, _T("[i]%s[/i] (%s)") },
		{ vkbbcUrl, bbcAdvanced, _T("[url=%s]%s[/url]") },
		{ vkbbcSize, bbcNo, _T("%s") },
		{ vkbbcSize, bbcBasic, _T("%s") },
		{ vkbbcSize, bbcAdvanced, _T("[size=%s]%s[/size]") },
		{ vkbbcColor, bbcNo, _T("%s") },
		{ vkbbcColor, bbcBasic, _T("%s") },
		{ vkbbcColor, bbcAdvanced, _T("[color=%s]%s[/color]") },
	};

	if (IsEmpty(ptszString))
		return CMString();

	TCHAR *ptszFormat = NULL;
	for (int i = 0; i < _countof(bbcItem); i++)
		if (bbcItem[i].vkBBCType == bbcType && bbcItem[i].vkBBCSettings == iBBC) {
			ptszFormat = bbcItem[i].ptszTempate;
			break;
		}

	CMString res;
	if (ptszFormat == NULL)
		return CMString(ptszString);

	if (bbcType == vkbbcUrl && iBBC != bbcAdvanced)
		res.AppendFormat(ptszFormat, ptszString, tszAddString ? tszAddString : _T(""));
	else if (iBBC == bbcAdvanced && bbcType >= vkbbcUrl)
		res.AppendFormat(ptszFormat, tszAddString ? tszAddString : _T(""), ptszString);
	else
		res.AppendFormat(ptszFormat, ptszString);

	return res;
}

CMString& CVkProto::ClearFormatNick(CMString& tszText)
{
	int iNameEnd = tszText.Find(_T("],")), iNameBeg = tszText.Find(_T("|"));
	if (iNameEnd != -1 && iNameBeg != -1 && iNameBeg < iNameEnd) {
		CMString tszName = tszText.Mid(iNameBeg + 1, iNameEnd - iNameBeg - 1);
		CMString tszBody = tszText.Mid(iNameEnd + 2);
		if (!tszName.IsEmpty() && !tszBody.IsEmpty())
			tszText = tszName + _T(",") + tszBody;
	}

	return tszText;
}

/////////////////////////////////////////////////////////////////////////////////////////

CMString CVkProto::GetAttachmentDescr(const JSONNode &jnAttachments, BBCSupport iBBC)
{
	debugLogA("CVkProto::GetAttachmentDescr");
	CMString res;
	if (!jnAttachments) {
		debugLogA("CVkProto::GetAttachmentDescr pAttachments == NULL");
		return res;
	}

	res += SetBBCString(TranslateT("Attachments:"), iBBC, vkbbcB);
	res.AppendChar('\n');
	
	for (auto it = jnAttachments.begin(); it != jnAttachments.end(); ++it) {
		const JSONNode &jnAttach = (*it);

		res.AppendChar('\t');
		CMString tszType(jnAttach["type"].as_mstring());
		if (tszType == _T("photo")) {
			const JSONNode &jnPhoto = jnAttach["photo"];
			if (!jnPhoto)
				continue;

			res += GetVkPhotoItem(jnPhoto, iBBC);
		}
		else if (tszType ==_T("audio")) {
			const JSONNode &jnAudio = jnAttach["audio"];
			if (!jnAudio)
				continue;

			CMString tszArtist(jnAudio["artist"].as_mstring());
			CMString tszTitle(jnAudio["title"].as_mstring());
			CMString tszUrl(jnAudio["url"].as_mstring());
			CMString tszAudio(FORMAT, _T("%s - %s"), tszArtist, tszTitle);

			int iParamPos = tszUrl.Find(_T("?"));
			if (m_vkOptions.bShortenLinksForAudio &&  iParamPos != -1)
				tszUrl = tszUrl.Left(iParamPos);

			res.AppendFormat(_T("%s: %s"),
				SetBBCString(TranslateT("Audio"), iBBC, vkbbcB),
				SetBBCString(tszAudio, iBBC, vkbbcUrl, tszUrl));
		}
		else if (tszType ==_T("video")) {
			const JSONNode &jnVideo = jnAttach["video"];
			if (!jnVideo)
				continue;

			CMString tszTitle(jnVideo["title"].as_mstring());
			int vid = jnVideo["id"].as_int();
			int ownerID = jnVideo["owner_id"].as_int();
			CMString tszUrl(FORMAT, _T("http://vk.com/video%d_%d"), ownerID, vid);
			res.AppendFormat(_T("%s: %s"),
				SetBBCString(TranslateT("Video"), iBBC, vkbbcB),
				SetBBCString(tszTitle, iBBC, vkbbcUrl, tszUrl));
		}
		else if (tszType == _T("doc")) {
			const JSONNode &jnDoc = jnAttach["doc"];
			if (!jnDoc)
				continue;

			CMString tszTitle(jnDoc["title"].as_mstring());
			CMString tszUrl(jnDoc["url"].as_mstring());
			res.AppendFormat(_T("%s: %s"),
				SetBBCString(TranslateT("Document"), iBBC, vkbbcB),
				SetBBCString(tszTitle, iBBC, vkbbcUrl, tszUrl));
		}
		else if (tszType == _T("wall")) {
			const JSONNode &jnWall = jnAttach["wall"];
			if (!jnWall)
				continue;

			CMString tszText(jnWall["text"].as_mstring());
			int id = jnWall["id"].as_int();
			int fromID = jnWall["from_id"].as_int();
			CMString tszUrl(FORMAT, _T("http://vk.com/wall%d_%d"), fromID, id);
			res.AppendFormat(_T("%s: %s"),
				SetBBCString(TranslateT("Wall post"), iBBC, vkbbcUrl, tszUrl),
				tszText.IsEmpty() ? _T(" ") : tszText);
			
			const JSONNode &jnCopyHystory = jnWall["copy_history"];
			for (auto aCHit = jnCopyHystory.begin(); aCHit != jnCopyHystory.end(); ++aCHit) {
				const JSONNode &jnCopyHystoryItem = (*aCHit);

				CMString tszCHText(jnCopyHystoryItem["text"].as_mstring());
				int iCHid = jnCopyHystoryItem["id"].as_int();
				int iCHfromID = jnCopyHystoryItem["from_id"].as_int();
				CMString tszCHUrl(FORMAT, _T("http://vk.com/wall%d_%d"), iCHfromID, iCHid);
				tszCHText.Replace(_T("\n"), _T("\n\t\t"));
				res.AppendFormat(_T("\n\t\t%s: %s"),
					SetBBCString(TranslateT("Wall post"), iBBC, vkbbcUrl, tszCHUrl),
					tszCHText.IsEmpty() ? _T(" ") : tszCHText);

				const JSONNode &jnSubAttachments = jnCopyHystoryItem["attachments"];
				if (jnSubAttachments) {
					debugLogA("CVkProto::GetAttachmentDescr SubAttachments");
					CMString tszAttachmentDescr = GetAttachmentDescr(jnSubAttachments, iBBC);
					tszAttachmentDescr.Replace(_T("\n"), _T("\n\t\t"));
					res += _T("\n\t\t") + tszAttachmentDescr;
				}
			}

			const JSONNode &jnSubAttachments = jnWall["attachments"];
			if (jnSubAttachments) {
				debugLogA("CVkProto::GetAttachmentDescr SubAttachments");
				CMString tszAttachmentDescr = GetAttachmentDescr(jnSubAttachments, iBBC);
				tszAttachmentDescr.Replace(_T("\n"), _T("\n\t"));
				res += _T("\n\t") + tszAttachmentDescr;
			}
		}
		else if (tszType == _T("sticker")) {
			const JSONNode &jnSticker = jnAttach["sticker"];
			if (!jnSticker)
				continue;
			res.Empty(); // sticker is not really an attachment, so we don't want all that heading info

			if (m_vkOptions.bStikersAsSmyles) {
				int id = jnSticker["id"].as_int();
				res.AppendFormat(_T("[sticker:%d]"), id);
			}
			else {
				CMString tszLink;
				for (int i = 0; i < _countof(szImageTypes); i++) {
					const JSONNode &n = jnSticker[szImageTypes[i]];
					if (n) {
						tszLink = n.as_mstring();
						break;
					}
				}
				res.AppendFormat(_T("%s"), tszLink);

				if (m_vkOptions.iIMGBBCSupport && iBBC != bbcNo)
					res += SetBBCString(tszLink, iBBC, vkbbcImg);
			}
		}
		else if (tszType == _T("link")) {
			const JSONNode &jnLink = jnAttach["link"];
			if (!jnLink)
				continue;

			CMString tszUrl(jnLink["url"].as_mstring());
			CMString tszTitle(jnLink["title"].as_mstring());
			CMString tszCaption(jnLink["caption"].as_mstring());
			CMString tszDescription(jnLink["description"].as_mstring());

			res.AppendFormat(_T("%s: %s"),
				SetBBCString(TranslateT("Link"), iBBC, vkbbcB),
				SetBBCString(tszTitle, iBBC, vkbbcUrl, tszUrl));
			
			if (!tszDescription.IsEmpty())
				res.AppendFormat(_T("\n\t%s"), SetBBCString(tszCaption, iBBC, vkbbcI));
			
			if (jnLink["photo"])		
				res.AppendFormat(_T("\n\t%s"), GetVkPhotoItem(jnLink["photo"], iBBC));

			if (!tszDescription.IsEmpty())
				res.AppendFormat(_T("\n\t%s"), tszDescription);
		}
		else if (tszType == _T("gift")) {
			const JSONNode &jnGift = jnAttach["gift"];
			if (!jnGift)
				continue;

			CMString tszLink;
			for (int i = 0; i < _countof(szGiftTypes); i++) {
				const JSONNode &n = jnGift[szGiftTypes[i]];
				if (n) {
					tszLink = n.as_mstring();
					break;
				}
			}
			if (tszLink.IsEmpty())
				continue;
			res += SetBBCString(TranslateT("Gift"), iBBC, vkbbcUrl, tszLink);

			if (m_vkOptions.iIMGBBCSupport && iBBC != bbcNo)
				res.AppendFormat(_T("\n\t%s"), SetBBCString(tszLink, iBBC, vkbbcImg));
		}
		else
			res.AppendFormat(TranslateT("Unsupported or unknown attachment type: %s"), SetBBCString(tszType, iBBC, vkbbcB));

		res.AppendChar('\n');
	}

	return res;
}

CMString CVkProto::GetFwdMessages(const JSONNode &jnMessages, const JSONNode &jnFUsers, BBCSupport iBBC)
{
	CMString res;
	debugLogA("CVkProto::GetFwdMessages");
	if (!jnMessages) {
		debugLogA("CVkProto::GetFwdMessages pMessages == NULL");
		return res;
	}

	OBJLIST<CVkUserInfo> vkUsers(2, NumericKeySortT);

	for (auto it = jnFUsers.begin(); it != jnFUsers.end(); ++it) {
		const JSONNode &jnUser = (*it);

		int iUserId = jnUser["id"].as_int();
		CMString tszNick(FORMAT, _T("%s %s"), jnUser["first_name"].as_mstring(), jnUser["last_name"].as_mstring());
		CMString tszLink(FORMAT, _T("https://vk.com/id%d"), iUserId);
		
		CVkUserInfo *vkUser = new CVkUserInfo(jnUser["id"].as_int(), false, tszNick, tszLink, FindUser(iUserId));
		vkUsers.insert(vkUser);
	}


	for (auto it = jnMessages.begin(); it != jnMessages.end(); ++it) {
		const JSONNode &jnMsg = (*it);

		UINT uid = jnMsg["user_id"].as_int();
		CVkUserInfo *vkUser = vkUsers.find((CVkUserInfo *)&uid);
		CMString tszNick, tszUrl;

		if (vkUser) {
			tszNick = vkUser->m_tszUserNick;
			tszUrl = vkUser->m_tszLink;
		} 
		else {
			MCONTACT hContact = FindUser(uid);
			if (hContact || uid == m_msgId)
				tszNick = ptrT(db_get_tsa(hContact, m_szModuleName, "Nick"));
			else 
				tszNick = TranslateT("(Unknown contact)");		
			tszUrl.AppendFormat(_T("https://vk.com/id%d"), uid);
		}

		time_t datetime = (time_t)jnMsg["date"].as_int();
		TCHAR ttime[64];
		_locale_t locale = _create_locale(LC_ALL, "");
		_tcsftime_l(ttime, _countof(ttime), _T("%x %X"), localtime(&datetime), locale);
		_free_locale(locale);

		CMString tszBody(jnMsg["body"].as_mstring());

		const JSONNode &jnFwdMessages = jnMsg["fwd_messages"];
		if (jnFwdMessages) {
			CMString tszFwdMessages = GetFwdMessages(jnFwdMessages, jnFUsers, iBBC == bbcNo ? iBBC : m_vkOptions.BBCForAttachments());
			if (!tszBody.IsEmpty())
				tszFwdMessages = _T("\n") + tszFwdMessages;
			tszBody += tszFwdMessages;
		}

		const JSONNode &jnAttachments = jnMsg["attachments"];
		if (jnAttachments) {
			CMString tszAttachmentDescr = GetAttachmentDescr(jnAttachments, iBBC == bbcNo ? iBBC : m_vkOptions.BBCForAttachments());
			if (!tszBody.IsEmpty())
				tszAttachmentDescr = _T("\n") + tszAttachmentDescr;
			tszBody += tszAttachmentDescr;
		}

		tszBody.Replace(_T("\n"), _T("\n\t"));
		TCHAR tcSplit = m_vkOptions.bSplitFormatFwdMsg ? '\n' : ' ';
		CMString tszMes(FORMAT, _T("%s %s%c%s %s:\n\n%s\n"),
			SetBBCString(TranslateT("Message from"), iBBC, vkbbcB),
			SetBBCString(tszNick, iBBC, vkbbcUrl, tszUrl),
			tcSplit,
			SetBBCString(TranslateT("at"), iBBC, vkbbcB),
			ttime,
			SetBBCString(tszBody, iBBC, vkbbcCode));

		if (!res.IsEmpty())
			res.AppendChar(_T('\n'));
		res += tszMes;
	}

	vkUsers.destroy();
	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::SetInvisible(MCONTACT hContact)
{
	debugLogA("CVkProto::SetInvisible %d", getDword(hContact, "ID", -1));
	if (getWord(hContact, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE) {
		setWord(hContact, "Status", ID_STATUS_INVISIBLE);
		SetMirVer(hContact, 1);
		db_set_dw(hContact, "BuddyExpectator", "LastStatus", ID_STATUS_INVISIBLE);
		debugLogA("CVkProto::SetInvisible %d set ID_STATUS_INVISIBLE", getDword(hContact, "ID", -1));
	}
	time_t now = time(NULL);
	db_set_dw(hContact, "BuddyExpectator", "LastSeen", (DWORD)now);
	setDword(hContact, "InvisibleTS", (DWORD)now);
}

CMString CVkProto::RemoveBBC(CMString& tszSrc) 
{
	static const TCHAR *tszSimpleBBCodes[][2] = {
		{ _T("[b]"), _T("[/b]") },
		{ _T("[u]"), _T("[/u]") },
		{ _T("[i]"), _T("[/i]") },
		{ _T("[s]"), _T("[/s]") },
	};

	static const TCHAR *tszParamBBCodes[][2] = {
		{ _T("[url="), _T("[/url]") },
		{ _T("[img="), _T("[/img]") },
		{ _T("[size="), _T("[/size]") },
		{ _T("[color="), _T("[/color]") },
	};

	CMString tszRes(tszSrc); 
	CMString tszLow(tszSrc); 
	tszLow.MakeLower();

	for (int i = 0; i < _countof(tszSimpleBBCodes); i++) {
		CMString tszOpenTag(tszSimpleBBCodes[i][0]);
		CMString tszCloseTag(tszSimpleBBCodes[i][1]);
		
		int lenOpen = tszOpenTag.GetLength();
		int lenClose = tszCloseTag.GetLength();

		int posOpen = 0;
		int posClose = 0;

		while (true) {
			if ((posOpen = tszLow.Find(tszOpenTag, posOpen)) < 0)
				break;

			if ((posClose = tszLow.Find(tszCloseTag, posOpen + lenOpen)) < 0)
				break;

			tszLow.Delete(posOpen, lenOpen);
			tszLow.Delete(posClose - lenOpen, lenClose);

			tszRes.Delete(posOpen, lenOpen);
			tszRes.Delete(posClose - lenOpen, lenClose);

		}
	}

	for (int i = 0; i < _countof(tszParamBBCodes); i++) {
		CMString tszOpenTag(tszParamBBCodes[i][0]);
		CMString tszCloseTag(tszParamBBCodes[i][1]);

		int lenOpen = tszOpenTag.GetLength();
		int lenClose = tszCloseTag.GetLength();

		int posOpen = 0;
		int posOpen2 = 0;
		int posClose = 0;

		while (true) {
			if ((posOpen = tszLow.Find(tszOpenTag, posOpen)) < 0)
				break;
			
			if ((posOpen2 = tszLow.Find(_T("]"), posOpen + lenOpen)) < 0)
				break;

			if ((posClose = tszLow.Find(tszCloseTag, posOpen2 + 1)) < 0)
				break;

			tszLow.Delete(posOpen, posOpen2 - posOpen + 1);
			tszLow.Delete(posClose - posOpen2 + posOpen - 1, lenClose);

			tszRes.Delete(posOpen, posOpen2 - posOpen + 1);
			tszRes.Delete(posClose - posOpen2 + posOpen - 1, lenClose);

		}

	}

	return tszRes;
}

void CVkProto::ShowCaptchaInBrowser(HBITMAP hBitmap)
{
	TCHAR tszTempDir[MAX_PATH];
	if (!GetEnvironmentVariable(_T("TEMP"), tszTempDir, MAX_PATH))
		return;

	CMString tszHTMLPath(FORMAT, _T("%s\\miranda_captcha.html"), tszTempDir);
		
	FILE *pFile = _tfopen(tszHTMLPath, _T("w"));
	if (pFile == NULL)
		return;

	FIBITMAP *dib = fii->FI_CreateDIBFromHBITMAP(hBitmap);
	FIMEMORY *hMem = fii->FI_OpenMemory(nullptr, 0);
	fii->FI_SaveToMemory(FIF_PNG, dib, hMem, 0);

	BYTE *buf = NULL;
	DWORD bufLen;
	fii->FI_AcquireMemory(hMem, &buf, &bufLen);
	ptrA base64(mir_base64_encode(buf, bufLen));
	fii->FI_CloseMemory(hMem);
	fii->FI_Unload(dib);

	CMStringA szHTML(FORMAT, "<html><body><img src=\"data:image/png;base64,%s\" /></body></html>", base64);
	fwrite(szHTML, 1, szHTML.GetLength(), pFile);
	fclose(pFile);

	tszHTMLPath = _T("file://") + tszHTMLPath;
	Utils_OpenUrlT(tszHTMLPath);
}