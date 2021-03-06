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

enum
{
	IDM_NONE,
	IDM_TOPIC, IDM_INVITE, IDM_DESTROY,
	IDM_KICK, IDM_INFO, IDM_CHANGENICK, IDM_VISIT_PROFILE
};

static LPCTSTR sttStatuses[] = { LPGENT("Participants"), LPGENT("Owners") };

extern JSONNode nullNode;

CVkChatInfo* CVkProto::AppendChat(int id, const JSONNode &jnDlg)
{
	debugLog(_T("CVkProto::AppendChat"));
	if (id == 0)
		return NULL;

	MCONTACT chatContact = FindChat(id);
	if (chatContact && getBool(chatContact, "kicked"))
		return NULL;

	CVkChatInfo *c = m_chats.find((CVkChatInfo*)&id);
	if (c != NULL)
		return c;

	CMString tszTitle;
	c = new CVkChatInfo(id);
	if (jnDlg) {
		tszTitle = jnDlg["title"].as_mstring();
		c->m_tszTopic = mir_tstrdup(!tszTitle.IsEmpty() ? tszTitle : _T(""));
	}

	CMString sid; 
	sid.Format(_T("%S_%d"), m_szModuleName, id);
	c->m_tszId = mir_tstrdup(sid);

	GCSESSION gcw = { sizeof(gcw) };
	gcw.iType = GCW_CHATROOM;
	gcw.pszModule = m_szModuleName;
	gcw.ptszName = tszTitle;
	gcw.ptszID = sid;
	CallServiceSync(MS_GC_NEWSESSION, NULL, (LPARAM)&gcw);

	GC_INFO gci = { 0 };
	gci.pszModule = m_szModuleName;
	gci.pszID = sid;
	gci.Flags = GCF_BYID | GCF_HCONTACT;
	CallServiceSync(MS_GC_GETINFO, 0, (LPARAM)&gci);
	c->m_hContact = gci.hContact;

	setTString(gci.hContact, "Nick", tszTitle);
	m_chats.insert(c);

	GCDEST gcd = { m_szModuleName, sid, GC_EVENT_ADDGROUP };
	GCEVENT gce = { sizeof(gce), &gcd };
	for (int i = _countof(sttStatuses)-1; i >= 0; i--) {
		gce.ptszStatus = TranslateTS(sttStatuses[i]);
		CallServiceSync(MS_GC_EVENT, NULL, (LPARAM)&gce);
	}

	setDword(gci.hContact, "vk_chat_id", id);

	CMString tszHomepage(FORMAT, _T("https://vk.com/im?sel=c%d"), id);
	setTString(gci.hContact, "Homepage", tszHomepage);
	
	db_unset(gci.hContact, m_szModuleName, "off");

	if (jnDlg["left"].as_bool())  {
		setByte(gci.hContact, "off", 1);
		m_chats.remove(c);
		return NULL;
	}
	gcd.iType = GC_EVENT_CONTROL;
	gce.ptszStatus = 0;
	CallServiceSync(MS_GC_EVENT, (m_vkOptions.bHideChats) ? WINDOW_HIDDEN : SESSION_INITDONE, (LPARAM)&gce);
	CallServiceSync(MS_GC_EVENT, SESSION_ONLINE, (LPARAM)&gce);

	RetrieveChatInfo(c);
	return c;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::RetrieveChatInfo(CVkChatInfo *cc)
{

	CMStringA tszQuery(FORMAT, "var ChatId=%d;", cc->m_chatid);
	tszQuery += "var Info=API.messages.getChat({\"chat_id\":ChatId});"
		"var ChatUsers=API.messages.getChatUsers({\"chat_id\":ChatId,\"fields\":\"id,first_name,last_name\"});";

	if (!cc->m_bHistoryRead) {
		tszQuery += "var ChatMsg=API.messages.getHistory({\"chat_id\":ChatId,\"count\":20,\"rev\":0});var UR=parseInt(ChatMsg.unread);"
			"if(UR>20){if(UR>200)UR=200;ChatMsg=API.messages.getHistory({\"chat_id\":ChatId,\"count\":UR,\"rev\":0});};"
			"var FMsgs = ChatMsg.items@.fwd_messages;var Idx = 0;var Uids =[];while (Idx < FMsgs.length){"
			"var Jdx = 0;var CFMsgs = parseInt(FMsgs[Idx].length);while (Jdx < CFMsgs){"
			"Uids.unshift(FMsgs[Idx][Jdx].user_id);Jdx = Jdx + 1;};Idx = Idx + 1;};"
			"var FUsers = API.users.get({\"user_ids\": Uids, \"name_case\":\"gen\"});"
			"var MsgUsers=API.users.get({\"user_ids\":ChatMsg.items@.user_id,\"fields\":\"id,first_name,last_name\"});";
	}

	tszQuery += "return {\"info\":Info,\"users\":ChatUsers";

	if (!cc->m_bHistoryRead)
		tszQuery += ",\"msgs\":ChatMsg,\"fwd_users\":FUsers,\"msgs_users\":MsgUsers";

	tszQuery +="};";

	debugLogA("CVkProto::RetrieveChantInfo(%d)", cc->m_chatid);
	if (!IsOnline())
		return;
	Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/execute.json", true, &CVkProto::OnReceiveChatInfo)
		<< CHAR_PARAM("code", tszQuery))->pUserInfo = cc;
}

void CVkProto::OnReceiveChatInfo(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnReceiveChatInfo %d", reply->resultCode);
	if (reply->resultCode != 200)
		return;

	JSONNode jnRoot;
	const JSONNode &jnResponse = CheckJsonResponse(pReq, reply, jnRoot);
	if (!jnResponse)
		return;

	CVkChatInfo *cc = (CVkChatInfo*)pReq->pUserInfo;
	if (m_chats.indexOf(cc) == -1)
		return;

	const JSONNode &jnInfo = jnResponse["info"];
	if (jnInfo) {
		if (jnInfo["title"])
			SetChatTitle(cc, jnInfo["title"].as_mstring());

		if (jnInfo["left"].as_bool() || jnInfo["kicked"].as_bool()) {
			setByte(cc->m_hContact, "kicked", jnInfo["kicked"].as_bool());
			LeaveChat(cc->m_chatid);
			return;
		}
		cc->m_admin_id = jnInfo["admin_id"].as_int();
	}

	const JSONNode &jnUsers = jnResponse["users"];
	if (jnUsers) {
		for (int i = 0; i < cc->m_users.getCount(); i++)
			cc->m_users[i].m_bDel = true;

		for (auto it = jnUsers.begin(); it != jnUsers.end(); ++it) {
			const JSONNode &jnUser = (*it);
			if (!jnUser)
				break;

			int uid = jnUser["id"].as_int();
			TCHAR tszId[20];
			_itot(uid, tszId, 10);

			bool bNew;
			CVkChatUser *cu = cc->m_users.find((CVkChatUser*)&uid);
			if (cu == NULL) {
				cc->m_users.insert(cu = new CVkChatUser(uid));
				bNew = true;
			}
			else 
				bNew = cu->m_bUnknown;
			cu->m_bDel = false;

			CMString tszNick(ptrT(db_get_tsa(cc->m_hContact, m_szModuleName, CMStringA(FORMAT, "nick%d", cu->m_uid))));
			if (tszNick.IsEmpty()) {
				CMString fName(jnUser["first_name"].as_mstring());
				CMString lName(jnUser["last_name"].as_mstring());
				tszNick = fName.Trim() + _T(" ") + lName.Trim();
			}
			cu->m_tszNick = mir_tstrdup(tszNick);
			cu->m_bUnknown = false;
			
			if (bNew) {
				GCDEST gcd = { m_szModuleName, cc->m_tszId, GC_EVENT_JOIN };
				GCEVENT gce = { sizeof(GCEVENT), &gcd };
				gce.bIsMe = uid == m_myUserId;
				gce.ptszUID = tszId;
				gce.ptszNick = tszNick;
				gce.ptszStatus = TranslateTS(sttStatuses[uid == cc->m_admin_id]);
				gce.dwItemData = (INT_PTR)cu;
				CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
			}
		}

		for (int i = cc->m_users.getCount() - 1; i >= 0; i--) {
			CVkChatUser &cu = cc->m_users[i];
			if (!cu.m_bDel)
				continue;

			TCHAR tszId[20];
			_itot(cu.m_uid, tszId, 10);

			GCDEST gcd = { m_szModuleName, cc->m_tszId, GC_EVENT_PART };
			GCEVENT gce = { sizeof(GCEVENT), &gcd };
			gce.ptszUID = tszId;
			gce.dwFlags = GCEF_REMOVECONTACT | GCEF_NOTNOTIFY;
			gce.time = time(NULL);
			gce.ptszNick = mir_tstrdup(CMString(FORMAT, _T("%s (https://vk.com/id%s)"), cu.m_tszNick, tszId));
			CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);

			cc->m_users.remove(i);
		}
	}

	const JSONNode &jnMsgsUsers = jnResponse["msgs_users"];
	for (auto it = jnMsgsUsers.begin(); it != jnMsgsUsers.end(); ++it) {
		const JSONNode &jnUser = (*it);
		LONG uid = jnUser["id"].as_int();
		CVkChatUser *cu = cc->m_users.find((CVkChatUser*)&uid);
		if (cu)
			continue;
		
		MCONTACT hContact = FindUser(uid);
		if (hContact)
			continue;

		hContact = SetContactInfo(jnUser, true);
		db_set_b(hContact, "CList", "Hidden", 1);
		db_set_b(hContact, "CList", "NotOnList", 1);
		db_set_dw(hContact, "Ignore", "Mask1", 0);
	}

	const JSONNode &jnMsgs = jnResponse["msgs"];
	const JSONNode &jnFUsers = jnResponse["fwd_users"];
	if (jnMsgs) {
		
		const JSONNode &jnItems = jnMsgs["items"];
		if (jnItems) {
			for (auto it = jnItems.begin(); it != jnItems.end(); ++it) {
				const JSONNode &jnMsg = (*it);
				if (!jnMsg)
					break;

				AppendChatMessage(cc->m_chatid, jnMsg, jnFUsers, true);
			}
			cc->m_bHistoryRead = true;
		}
	}

	for (int j = 0; j < cc->m_msgs.getCount(); j++) {
		CVkChatMessage &p = cc->m_msgs[j];
		AppendChatMessage(cc, p.m_uid, p.m_date, p.m_tszBody, p.m_bHistory, p.m_bIsAction);
	}
	cc->m_msgs.destroy();
}

void CVkProto::SetChatTitle(CVkChatInfo *cc, LPCTSTR tszTopic)
{
	debugLog(_T("CVkProto::SetChatTitle"));
	if (!cc)
		return;

	if (mir_tstrcmp(cc->m_tszTopic, tszTopic) == 0)
		return;

	cc->m_tszTopic = mir_tstrdup(tszTopic);
	setTString(cc->m_hContact, "Nick", tszTopic);

	GCDEST gcd = { m_szModuleName, cc->m_tszId, GC_EVENT_CHANGESESSIONAME };
	GCEVENT gce = { sizeof(GCEVENT), &gcd };
	gce.ptszText = tszTopic;
	CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::AppendChatMessage(int id, const JSONNode &jnMsg, const JSONNode &jnFUsers, bool bIsHistory)
{
	debugLogA("CVkProto::AppendChatMessage");
	CVkChatInfo *cc = AppendChat(id, nullNode);
	if (cc == NULL)
		return;

	int mid = jnMsg["id"].as_int();
	int uid = jnMsg["user_id"].as_int();
	bool bIsAction = false;

	int msgTime = jnMsg["date"].as_int();
	time_t now = time(NULL);
	if (!msgTime || msgTime > now)
		msgTime = now;

	CMString tszBody(jnMsg["body"].as_mstring());

	const JSONNode &jnFwdMessages = jnMsg["fwd_messages"];
	if (jnFwdMessages) {
		CMString tszFwdMessages = GetFwdMessages(jnFwdMessages, jnFUsers, bbcNo);
		if (!tszBody.IsEmpty())
			tszFwdMessages = _T("\n") + tszFwdMessages;
		tszBody += tszFwdMessages;
	}

	const JSONNode &jnAttachments = jnMsg["attachments"];
	if (jnAttachments) {
		CMString tszAttachmentDescr = GetAttachmentDescr(jnAttachments, bbcNo);
		if (!tszBody.IsEmpty())
			tszAttachmentDescr = _T("\n") + tszAttachmentDescr;
		tszBody += tszAttachmentDescr;
	}

	if (jnMsg["action"]) {
		bIsAction = true;
		CMString tszAction = jnMsg["action"].as_mstring();
		
		if (tszAction == _T("chat_create")) {
			CMString tszActionText = jnMsg["action_text"].as_mstring();
			tszBody.AppendFormat(_T("%s \"%s\""), TranslateT("create chat"), tszActionText.IsEmpty() ? _T(" ") : tszActionText);
		}
		else if (tszAction == _T("chat_kick_user")) {
			CMString tszActionMid = jnMsg["action_mid"].as_mstring();
			if (tszActionMid.IsEmpty())
				tszBody = TranslateT("kick user");
			else {
				CMString tszUid(FORMAT, _T("%d"), uid);
				if (tszUid == tszActionMid) {
					if (cc->m_bHistoryRead)
						return;
					tszBody.AppendFormat(_T(" (https://vk.com/id%s) %s"), tszUid, TranslateT("left chat"));
				}
				else {
					int a_uid = 0;
					int iReadCount = _stscanf(tszActionMid, _T("%d"), &a_uid);
					if (iReadCount == 1) {
						CVkChatUser *cu = cc->m_users.find((CVkChatUser*)&a_uid);
						if (cu == NULL)
							tszBody.AppendFormat(_T("%s (https://vk.com/id%d)"), TranslateT("kick user"), a_uid);
						else
							tszBody.AppendFormat(_T("%s %s (https://vk.com/id%d)"), TranslateT("kick user"), cu->m_tszNick, a_uid);
					}
					else 
						tszBody = TranslateT("kick user");
				}
			}
		}
		else if (tszAction == _T("chat_invite_user")) {
			CMString tszActionMid = jnMsg["action_mid"].as_mstring();
			if (tszActionMid.IsEmpty())
				tszBody = TranslateT("invite user");
			else {
				CMString tszUid(FORMAT, _T("%d"), uid);
				if (tszUid == tszActionMid)
					tszBody.AppendFormat(_T(" (https://vk.com/id%s) %s"), tszUid, TranslateT("returned to chat"));
				else {
					int a_uid = 0;
					int iReadCount = _stscanf(tszActionMid, _T("%d"), &a_uid);
					if (iReadCount == 1) {
						CVkChatUser *cu = cc->m_users.find((CVkChatUser*)&a_uid);
						if (cu == NULL)
							tszBody.AppendFormat(_T("%s (https://vk.com/id%d)"), TranslateT("invite user"), a_uid);
						else
							tszBody.AppendFormat(_T("%s %s (https://vk.com/id%d)"), TranslateT("invite user"), cu->m_tszNick, a_uid);
					}
					else
						tszBody = TranslateT("invite user");
				}
			}
		}
		else if (tszAction == _T("chat_title_update")) {
			CMString tszTitle = jnMsg["action_text"].as_mstring();
			tszBody.AppendFormat(_T("%s \"%s\""), TranslateT("change chat title to"), tszTitle.IsEmpty() ? _T(" ") : tszTitle);

			if (!bIsHistory)
				SetChatTitle(cc, tszTitle);
		}
		else if (tszAction == _T("chat_photo_update"))
			tszBody.Replace(TranslateT("Attachments:"), TranslateT("changed chat cover:"));
		else if (tszAction == _T("chat_photo_remove"))
			tszBody = TranslateT("deleted chat cover");
		else
			tszBody.AppendFormat(_T(": %s (%s)"), TranslateT("chat action not supported"), tszAction);
	}

	tszBody.Replace(_T("%"), _T("%%"));

	if (cc->m_bHistoryRead) {
		if (jnMsg["title"])
			SetChatTitle(cc, jnMsg["title"].as_mstring());
		AppendChatMessage(cc, uid, msgTime, tszBody, bIsHistory, bIsAction);
	}
	else {
		CVkChatMessage *cm = cc->m_msgs.find((CVkChatMessage *)&mid);
		if (cm == NULL)
			cc->m_msgs.insert(cm = new CVkChatMessage(mid));

		cm->m_uid = uid;
		cm->m_date = msgTime;
		cm->m_tszBody = mir_tstrdup(tszBody);
		cm->m_bHistory = bIsHistory;
		cm->m_bIsAction = bIsAction;
	}
}

void CVkProto::AppendChatMessage(CVkChatInfo *cc, int uid, int msgTime, LPCTSTR ptszBody, bool bIsHistory, bool bIsAction)
{
	debugLogA("CVkProto::AppendChatMessage2");
	MCONTACT hContact = FindUser(uid);
	CVkChatUser *cu = cc->m_users.find((CVkChatUser*)&uid);
	if (cu == NULL) {
		cc->m_users.insert(cu = new CVkChatUser(uid));
		CMString tszNick(ptrT(db_get_tsa(cc->m_hContact, m_szModuleName, CMStringA(FORMAT, "nick%d", cu->m_uid))));
		cu->m_tszNick = mir_tstrdup(tszNick.IsEmpty() ? (hContact ? ptrT(db_get_tsa(hContact, m_szModuleName, "Nick")) : TranslateT("Unknown")) : tszNick);
		cu->m_bUnknown = true;
	}

	TCHAR tszId[20];
	_itot(uid, tszId, 10);

	GCDEST gcd = { m_szModuleName, cc->m_tszId, bIsAction ? GC_EVENT_ACTION : GC_EVENT_MESSAGE };
	GCEVENT gce = { sizeof(GCEVENT), &gcd };
	gce.bIsMe = (uid == m_myUserId);
	gce.ptszUID = tszId;
	gce.time = msgTime;
	gce.dwFlags = (bIsHistory) ? GCEF_NOTNOTIFY : GCEF_ADDTOLOG;
	gce.ptszNick = cu->m_tszNick ? mir_tstrdup(cu->m_tszNick) : mir_tstrdup(hContact ? ptrT(db_get_tsa(hContact, m_szModuleName, "Nick")) : TranslateT("Unknown"));
	gce.ptszText = IsEmpty((TCHAR *)ptszBody) ? mir_tstrdup(_T("...")) : mir_tstrdup(ptszBody);
	CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
	StopChatContactTyping(cc->m_chatid, uid);
}

/////////////////////////////////////////////////////////////////////////////////////////

CVkChatInfo* CVkProto::GetChatById(LPCTSTR ptszId)
{
	for (int i = 0; i < m_chats.getCount(); i++)
		if (!mir_tstrcmp(m_chats[i].m_tszId, ptszId))
			return &m_chats[i];

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::SetChatStatus(MCONTACT hContact, int iStatus)
{
	ptrT tszChatID(getTStringA(hContact, "ChatRoomID"));
	if (tszChatID == NULL)
		return;

	CVkChatInfo *cc = GetChatById(tszChatID);
	if (cc == NULL)
		return;

	GCDEST gcd = { m_szModuleName, tszChatID, GC_EVENT_CONTROL };
	GCEVENT gce = { sizeof(gce), &gcd };
	CallServiceSync(MS_GC_EVENT, (iStatus == ID_STATUS_OFFLINE) ? SESSION_OFFLINE : SESSION_ONLINE, (LPARAM)&gce);
}

/////////////////////////////////////////////////////////////////////////////////////////

TCHAR* UnEscapeChatTags(TCHAR *str_in)
{
	TCHAR *s = str_in, *d = str_in;
	while (*s) {
		if (*s == '%' && s[1] == '%')
			s++;
		*d++ = *s++;
	}
	*d = 0;
	return str_in;
}

int CVkProto::OnChatEvent(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK*)lParam;
	if (gch == NULL)
		return 0;

	if (mir_strcmpi(gch->pDest->pszModule, m_szModuleName))
		return 0;

	CVkChatInfo *cc = GetChatById(gch->pDest->ptszID);
	if (cc == NULL)
		return 0;

	switch (gch->pDest->iType) {
	case GC_USER_MESSAGE:
		if (IsOnline() && mir_tstrlen(gch->ptszText) > 0) {
			ptrT ptszBuf(mir_tstrdup(gch->ptszText));
			rtrimt(ptszBuf);
			UnEscapeChatTags(ptszBuf);
			SendMsg(cc->m_hContact, 0, T2Utf(ptszBuf));
		}
		break;

	case GC_USER_PRIVMESS:
		{
			MCONTACT hContact = FindUser(_ttoi(gch->ptszUID));
			if (hContact == NULL) {
				hContact = FindUser(_ttoi(gch->ptszUID), true);
				db_set_b(hContact, "CList", "Hidden", 1);
				db_set_b(hContact, "CList", "NotOnList", 1);
				db_set_dw(hContact, "Ignore", "Mask1", 0);
				RetrieveUserInfo(_ttoi(gch->ptszUID));
			}
			CallService(MS_MSG_SENDMESSAGET, hContact);
		}
		break;

	case GC_USER_LOGMENU:
		LogMenuHook(cc, gch);
		break;

	case GC_USER_NICKLISTMENU:
		NickMenuHook(cc, gch);
		break;
	}
	return 0;
}

void CVkProto::OnSendChatMsg(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnSendChatMsg %d", reply->resultCode);
	int iResult = ACKRESULT_FAILED;
	if (reply->resultCode == 200) {
		JSONNode jnRoot;
		CheckJsonResponse(pReq, reply, jnRoot);
		iResult = ACKRESULT_SUCCESS;
	}
	if (!pReq->pUserInfo)
		return;
	
	CVkFileUploadParam *fup = (CVkFileUploadParam *)pReq->pUserInfo;
	ProtoBroadcastAck(fup->hContact, ACKTYPE_FILE, iResult, (HANDLE)(fup));
	if (!pReq->bNeedsRestart || m_bTerminated) {
		delete fup;
		pReq->pUserInfo = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

LPTSTR CVkProto::ChangeChatTopic(CVkChatInfo *cc)
{
	ENTER_STRING pForm = { sizeof(pForm) };
	pForm.type = ESF_MULTILINE;
	pForm.caption = TranslateT("Enter new chat title");
	pForm.ptszInitVal = cc->m_tszTopic;
	pForm.szModuleName = m_szModuleName;
	pForm.szDataPrefix = "gctopic_";
	return (!EnterString(&pForm)) ? NULL : pForm.ptszResult;
}

void CVkProto::LogMenuHook(CVkChatInfo *cc, GCHOOK *gch)
{
	if (!IsOnline())
		return;

	switch (gch->dwData) {
	case IDM_TOPIC:
		if (LPTSTR ptszNew = ChangeChatTopic(cc)) {
			Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/messages.editChat.json", true, &CVkProto::OnReceiveSmth)
				<< TCHAR_PARAM("title", ptszNew) 
				<< INT_PARAM("chat_id", cc->m_chatid));
			mir_free(ptszNew);
		}
		break;

	case IDM_INVITE:
		{
			CVkInviteChatForm dlg(this);
			if (dlg.DoModal() && dlg.m_hContact != NULL) {
				int uid = getDword(dlg.m_hContact, "ID", -1);
				if (uid != -1)
					Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/messages.addChatUser.json", true, &CVkProto::OnReceiveSmth)
						<< INT_PARAM("user_id", uid)
						<< INT_PARAM("chat_id", cc->m_chatid));
			}
		}
		break;

	case IDM_DESTROY:
		if (IDYES == MessageBox(NULL,
			TranslateT("This chat is going to be destroyed forever with all its contents. This action cannot be undone. Are you sure?"),
			TranslateT("Warning"), MB_YESNOCANCEL | MB_ICONQUESTION))
		{
			CMStringA code;
			code.Format("API.messages.removeChatUser({\"chat_id\":%d, \"user_id\":%d});"
				"var Hist = API.messages.getHistory({\"chat_id\":%d, \"count\":200});"
				"var countMsg = Hist.count;var itemsMsg = Hist.items@.id; "
				"while (countMsg > 0) { API.messages.delete({\"message_ids\":itemsMsg});"
				"Hist=API.messages.getHistory({\"chat_id\":%d, \"count\":200});"
				"countMsg = Hist.count;itemsMsg = Hist.items@.id;}; return 1;", cc->m_chatid, m_myUserId, cc->m_chatid, cc->m_chatid);
			Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/execute.json", true, &CVkProto::OnChatDestroy)
				<< CHAR_PARAM("code", code))->pUserInfo = cc;
		}
		break;
	}
}

INT_PTR __cdecl CVkProto::OnJoinChat(WPARAM hContact, LPARAM)
{
	debugLogA("CVkProto::OnJoinChat");
	if (!IsOnline())
		return 1;

	if (getBool(hContact, "kicked"))
		return 1;
	
	if (!getBool(hContact, "off"))
		return 1;

	int chat_id = getDword(hContact, "vk_chat_id", -1);
	
	if (chat_id == -1)
		return 1;

	AsyncHttpRequest *pReq = new AsyncHttpRequest(this, REQUEST_POST, "/method/messages.send.json", true, &CVkProto::OnSendChatMsg, AsyncHttpRequest::rpHigh)
		<< INT_PARAM("chat_id", chat_id)
		<< TCHAR_PARAM("message", m_vkOptions.ptszReturnChatMessage);
	pReq->AddHeader("Content-Type", "application/x-www-form-urlencoded");
	Push(pReq);
	db_unset(hContact, m_szModuleName, "off");
	return 0;
}

INT_PTR __cdecl CVkProto::OnLeaveChat(WPARAM hContact, LPARAM)
{
	debugLogA("CVkProto::OnLeaveChat");
	if (!IsOnline())
		return 1;

	ptrT tszChatID(getTStringA(hContact, "ChatRoomID"));
	if (tszChatID == NULL)
		return 1;

	CVkChatInfo *cc = GetChatById(tszChatID);
	if (cc == NULL)
		return 1;
	
	Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/messages.removeChatUser.json", true, &CVkProto::OnChatLeave)
		<< INT_PARAM("chat_id", cc->m_chatid)
		<< INT_PARAM("user_id", m_myUserId))->pUserInfo = cc;

	return 0;
}

void CVkProto::LeaveChat(int chat_id, bool close_window, bool delete_chat)
{
	debugLogA("CVkProto::LeaveChat");
	CVkChatInfo *cc = (CVkChatInfo*)m_chats.find((CVkChatInfo*)&chat_id);
	if (cc == NULL)
		return;

	GCDEST gcd = { m_szModuleName, cc->m_tszId, GC_EVENT_QUIT };
	GCEVENT gce = { sizeof(GCEVENT), &gcd };
	CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
	gcd.iType = GC_EVENT_CONTROL;
	CallServiceSync(MS_GC_EVENT, close_window? SESSION_TERMINATE:SESSION_OFFLINE, (LPARAM)&gce);
	if (delete_chat)
		CallService(MS_DB_CONTACT_DELETE, (WPARAM)cc->m_hContact);
	else
		setByte(cc->m_hContact, "off", (int)true);
	m_chats.remove(cc);
}

void CVkProto::KickFromChat(int chat_id, int user_id, const JSONNode &jnMsg, const JSONNode &jnFUsers)
{
	debugLogA("CVkProto::KickFromChat (%d)", user_id);

	MCONTACT chatContact = FindChat(chat_id);
	if (chatContact && getBool(chatContact, "off"))
		return;

	if (user_id == m_myUserId)
		LeaveChat(chat_id);

	CVkChatInfo *cc = (CVkChatInfo*)m_chats.find((CVkChatInfo*)&chat_id);
	if (cc == NULL)
		return;

	MCONTACT hContact = FindUser(user_id, false);
	CMString msg(jnMsg["body"].as_mstring());
	if (msg.IsEmpty()) {
		msg = TranslateT("You've been kicked by ");
		if (hContact != NULL)
			msg += ptrT(db_get_tsa(hContact, m_szModuleName, "Nick"));
		else
			msg += TranslateT("(Unknown contact)");
	}
	else 
		AppendChatMessage(chat_id, jnMsg, jnFUsers, false);

	MsgPopup(hContact, msg, TranslateT("Chat"));
	setByte(cc->m_hContact, "kicked", 1);
	LeaveChat(chat_id);
}

void CVkProto::OnChatLeave(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnChatLeave %d", reply->resultCode);
	if (reply->resultCode != 200)
		return;

	CVkChatInfo *cc = (CVkChatInfo*)pReq->pUserInfo;
	LeaveChat(cc->m_chatid);
}

INT_PTR __cdecl CVkProto::SvcDestroyKickChat(WPARAM hContact, LPARAM)
{
	debugLogA("CVkProto::SvcDestroyKickChat");
	if (!IsOnline())
		return 1;

	if (!getBool(hContact, "off"))
		return 1;
		
	int chat_id = getDword(hContact, "vk_chat_id", -1);
	if (chat_id == -1) 
		return 1;

	CMStringA code;
	code.Format("var Hist = API.messages.getHistory({\"chat_id\":%d, \"count\":200});"
		"var countMsg = Hist.count;var itemsMsg = Hist.items@.id; "
		"while (countMsg > 0) { API.messages.delete({\"message_ids\":itemsMsg});"
		"Hist=API.messages.getHistory({\"chat_id\":%d, \"count\":200});"
		"countMsg = Hist.count;itemsMsg = Hist.items@.id;}; return 1;", chat_id, chat_id);
	Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/execute.json", true, &CVkProto::OnReceiveSmth)
		<< CHAR_PARAM("code", code));

	CallService(MS_DB_CONTACT_DELETE, (WPARAM)hContact);

	return 0;
}

void CVkProto::OnChatDestroy(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnChatDestroy %d", reply->resultCode);
	if (reply->resultCode != 200)
		return;

	CVkChatInfo *cc = (CVkChatInfo*)pReq->pUserInfo;
	LeaveChat(cc->m_chatid, true, true);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::NickMenuHook(CVkChatInfo *cc, GCHOOK *gch)
{
	CVkChatUser *cu = cc->GetUserById(gch->ptszUID);
	MCONTACT hContact;
	if (cu == NULL)
		return;

	char szUid[20], szChatId[20];
	_itoa(cu->m_uid, szUid, 10);
	_itoa(cc->m_chatid, szChatId, 10);

	switch (gch->dwData) {
	case IDM_INFO:
		hContact = FindUser(cu->m_uid);
		if (hContact == NULL) {
			hContact = FindUser(cu->m_uid, true);
			db_set_b(hContact, "CList", "Hidden", 1);
			db_set_b(hContact, "CList", "NotOnList", 1);
			db_set_dw(hContact, "Ignore", "Mask1", 0);
		}
		CallService(MS_USERINFO_SHOWDIALOG, hContact);
		break;

	case IDM_VISIT_PROFILE:
		hContact = FindUser(cu->m_uid);
		if (hContact == NULL)
			Utils_OpenUrlT(CMString(FORMAT, _T("http://vk.com/id%d"), cu->m_uid));
		else 
			SvcVisitProfile(hContact, 0);
		break;

	case IDM_CHANGENICK:
	{
		CMString tszNewNick = RunRenameNick(cu->m_tszNick);
		if (tszNewNick.IsEmpty() || tszNewNick == cu->m_tszNick)
			break;

		GCDEST gcd = { m_szModuleName, cc->m_tszId, GC_EVENT_NICK };
		GCEVENT gce = { sizeof(GCEVENT), &gcd };

		TCHAR tszId[20];
		_itot(cu->m_uid, tszId, 10);

		gce.ptszNick = mir_tstrdup(cu->m_tszNick);
		gce.bIsMe = (cu->m_uid == m_myUserId);
		gce.ptszUID = tszId;
		gce.ptszText = mir_tstrdup(tszNewNick);
		gce.dwFlags = GCEF_ADDTOLOG;
		gce.time = time(NULL);
		CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);

		cu->m_tszNick = mir_tstrdup(tszNewNick);		
		setTString(cc->m_hContact, CMStringA(FORMAT, "nick%d", cu->m_uid), tszNewNick);
	}
		break;

	case IDM_KICK:
		if (!IsOnline())
			return;

		Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/messages.removeChatUser.json", true, &CVkProto::OnReceiveSmth)
			<< INT_PARAM("chat_id", cc->m_chatid) 
			<< INT_PARAM("user_id", cu->m_uid));
		cu->m_bUnknown = true;

		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

static gc_item sttLogListItems[] =
{
	{ LPGENT("&Invite a user"), IDM_INVITE, MENU_ITEM },
	{ LPGENT("View/change &title"), IDM_TOPIC, MENU_ITEM },
	{ NULL, 0, MENU_SEPARATOR },
	{ LPGENT("&Destroy room"), IDM_DESTROY, MENU_ITEM }
};

static gc_item sttListItems[] =
{
	{ LPGENT("&User details"), IDM_INFO, MENU_ITEM },
	{ LPGENT("Visit profile"), IDM_VISIT_PROFILE, MENU_ITEM },
	{ LPGENT("Change nick"), IDM_CHANGENICK, MENU_ITEM },
	{ LPGENT("&Kick"), IDM_KICK, MENU_ITEM }
};

int CVkProto::OnGcMenuHook(WPARAM, LPARAM lParam)
{
	GCMENUITEMS *gcmi = (GCMENUITEMS*)lParam;
	if (gcmi == NULL)
		return 0;

	if (mir_strcmpi(gcmi->pszModule, m_szModuleName))
		return 0;

	if (gcmi->Type == MENU_ON_LOG) {
		gcmi->nItems = _countof(sttLogListItems);
		gcmi->Item = sttLogListItems;
	}
	else if (gcmi->Type == MENU_ON_NICKLIST) {
		gcmi->nItems = _countof(sttListItems);
		gcmi->Item = sttListItems;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CVkProto::ChatContactTypingThread(void *p)
{
	CVKChatContactTypingParam *param = (CVKChatContactTypingParam *)p;
	if (!p)
		return;

	int iChatId = param->m_ChatId;
	int iUserId = param->m_UserId;

	debugLogA("CVkProto::ChatContactTypingThread %d %d", iChatId, iUserId);

	MCONTACT hChatContact = FindChat(iChatId);
	if (hChatContact && getBool(hChatContact, "off")) {
		delete param;
		return;
	}
	CVkChatInfo *cc = (CVkChatInfo*)m_chats.find((CVkChatInfo*)&iChatId);
	if (cc == NULL) {
		delete param;
		return;
	}
	CVkChatUser *cu = cc->GetUserById(iUserId);
	if (cu == NULL) {
		delete param;
		return;
	}
	
	{
		mir_cslock lck(m_csChatTyping);
		CVKChatContactTypingParam *cp = (CVKChatContactTypingParam *)m_ChatsTyping.find((CVKChatContactTypingParam *)&iChatId);
		if (cp != NULL)
			m_ChatsTyping.remove(cp);	
		m_ChatsTyping.insert(param);
		
		StatusTextData st = { 0 };
		st.cbSize = sizeof(st);
		mir_sntprintf(st.tszText, TranslateT("%s is typing a message..."), cu->m_tszNick);

		CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hChatContact, (LPARAM)&st);
	}

	Sleep(9500);
	StopChatContactTyping(iChatId, iUserId);
}

void CVkProto::StopChatContactTyping(int iChatId, int iUserId)
{
	debugLogA("CVkProto::StopChatContactTyping %d %d", iChatId, iUserId);
	MCONTACT hChatContact = FindChat(iChatId);
	if (hChatContact && getBool(hChatContact, "off")) 
		return;
	
	CVkChatInfo *cc = (CVkChatInfo*)m_chats.find((CVkChatInfo*)&iChatId);
	if (cc == NULL)
		return;
	
	CVkChatUser *cu = cc->GetUserById(iUserId);
	if (cu == NULL)
		return;
	
	mir_cslock lck(m_csChatTyping);
	CVKChatContactTypingParam *cp = (CVKChatContactTypingParam *)m_ChatsTyping.find((CVKChatContactTypingParam *)&iChatId);

	if (cp != NULL && cp->m_UserId == iUserId) {
		m_ChatsTyping.remove(cp);

		StatusTextData st = { 0 };
		st.cbSize = sizeof(st);
		mir_sntprintf(st.tszText, _T(" "));
		
		// CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hChatContact, NULL) clears statusbar very slowly. 
		// (1-10 sec(!!!) for me on tabSRMM O_o)
		// So I call MS_MSG_SETSTATUSTEXT with st.tszText = " " for cleaning of "... is typing a message..." string.
		// It works instantly!
		
		CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hChatContact, (LPARAM)&st);
				
		// After that I call standard cleaning procedure:

		CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hChatContact);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CVkProto::SvcCreateChat(WPARAM, LPARAM)
{
	if (!IsOnline())
		return (INT_PTR)1;
	CVkGCCreateForm dlg(this);
	return (INT_PTR)!dlg.DoModal();
}

void CVkProto::CreateNewChat(LPCSTR uids, LPCTSTR ptszTitle)
{
	if (!IsOnline())
		return;
	Push(new AsyncHttpRequest(this, REQUEST_GET, "/method/messages.createChat.json", true, &CVkProto::OnCreateNewChat)
		<< TCHAR_PARAM("title", ptszTitle ? ptszTitle : _T(""))
		<< CHAR_PARAM("user_ids", uids));
}

void CVkProto::OnCreateNewChat(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnCreateNewChat %d", reply->resultCode);
	if (reply->resultCode != 200)
		return;

	JSONNode jnRoot;
	const JSONNode &jnResponse = CheckJsonResponse(pReq, reply, jnRoot);
	if (!jnResponse)
		return;

	int chat_id = jnResponse.as_int();
	if (chat_id != 0)
		AppendChat(chat_id, nullNode);
}