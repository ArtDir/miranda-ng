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

#pragma once

#define PS_CREATECHAT "/CreateNewChat"
#define PS_LOADVKNEWS "/LoadVKNews"
#define PS_WIPENONFRIENDS "/WipeNonFriendContacts"
#define PS_GETSERVERHISTORY "/SyncHistory"
#define PS_GETSERVERHISTORYLAST1DAY "/GetServerHystoryLast1Day"
#define PS_GETSERVERHISTORYLAST3DAY "/GetServerHystoryLast3Day"
#define PS_GETSERVERHISTORYLAST7DAY "/GetServerHystoryLast7Day"
#define PS_GETSERVERHISTORYLAST30DAY "/GetServerHystoryLast30Day"
#define PS_GETSERVERHISTORYLAST90DAY "/GetServerHystoryLast90Day"
#define PS_GETALLSERVERHISTORYFORCONTACT "/GetAllServerHystoryForContact"
#define PS_GETALLSERVERHISTORY "/GetAllServerHystory"
#define PS_VISITPROFILE "/VisitProfile"
#define PS_ADDASFRIEND "/AddAsFriend"
#define PS_DELETEFRIEND "/DeleteFriend"
#define PS_BANUSER "/BanUser"
#define PS_REPORTABUSE "/ReportAbuse"
#define PS_DESTROYKICKCHAT "/DestroyKickChat"
#define PS_OPENBROADCAST "/OpenBroadcast"
#define PS_SETSTATUSMSG "/SetStatusMsg"
#define PS_WALLPOST "/WallPost"
#define PS_MARKMESSAGESASREAD "/MarkMessagesAsRead"

#define MAXHISTORYMIDSPERONE 100
#define MAX_RETRIES 10
#define MAX_CONTACTS_PER_REQUEST 1000

struct CVkProto : public PROTO<CVkProto>
{
	CVkProto(const char*, const TCHAR*);
	~CVkProto();

//====================================================================================
// PROTO_INTERFACE
//====================================================================================

	virtual	MCONTACT __cdecl AddToList(int flags, PROTOSEARCHRESULT *psr);
	virtual	int __cdecl Authorize(MEVENT hDbEvent);
	virtual	int __cdecl AuthDeny(MEVENT hDbEvent, const TCHAR *szReason);
	virtual	int __cdecl AuthRequest(MCONTACT hContact, const TCHAR *szMessage);
	virtual	DWORD_PTR __cdecl GetCaps(int type, MCONTACT hContact = NULL);
	virtual	int __cdecl GetInfo(MCONTACT hContact, int infoType);
	virtual	HANDLE __cdecl SearchBasic(const TCHAR *id);
	virtual	HANDLE __cdecl SearchByEmail(const TCHAR *email);
	virtual	HANDLE __cdecl SearchByName(const TCHAR *nick, const TCHAR *firstName, const TCHAR *lastName);
	virtual	int __cdecl RecvMsg(MCONTACT hContact, PROTORECVEVENT*);
	virtual	int __cdecl SendMsg(MCONTACT hContact, int flags, const char *msg);
	virtual	HANDLE __cdecl SendFile(MCONTACT hContact, const TCHAR *szDescription, TCHAR **ppszFiles);
	virtual	int __cdecl SetStatus(int iNewStatus);
	virtual	int __cdecl UserIsTyping(MCONTACT hContact, int type);
	virtual	int __cdecl OnEvent(PROTOEVENTTYPE eventType, WPARAM wParam, LPARAM lParam);

//==== Events ========================================================================

	int __cdecl OnModulesLoaded(WPARAM, LPARAM);
	int __cdecl OnOptionsInit(WPARAM, LPARAM);
	int __cdecl OnPreShutdown(WPARAM, LPARAM);
	void OnOAuthAuthorize(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReceiveAvatar(NETLIBHTTPREQUEST*, AsyncHttpRequest*);

//==== Services ======================================================================

	INT_PTR __cdecl SvcCreateAccMgrUI(WPARAM, LPARAM);
	INT_PTR __cdecl SvcGetAvatarInfo(WPARAM, LPARAM);
	INT_PTR __cdecl SvcGetAvatarCaps(WPARAM, LPARAM);
	INT_PTR __cdecl SvcGetMyAvatar(WPARAM, LPARAM);
	INT_PTR __cdecl SvcSetListeningTo(WPARAM, LPARAM);

//==== Menus ==========================================================================

	INT_PTR __cdecl SvcVisitProfile(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcAddAsFriend(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcWipeNonFriendContacts(WPARAM, LPARAM);
	INT_PTR __cdecl SvcDeleteFriend(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcBanUser(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcReportAbuse(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcDestroyKickChat(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcOpenBroadcast(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcWallPost(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcMarkMessagesAsRead(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcSetStatusMsg(WPARAM, LPARAM);

//==== History Menus ==================================================================

	template <unsigned short Days>
	INT_PTR __cdecl SvcGetServerHistoryLastNDay(WPARAM hContact, LPARAM)
	{
		debugLogA("CVkProto::SvcGetServerHistoryLast%dDay", Days);
		if (!IsOnline())
			return 0;
		GetServerHistoryLastNDay(hContact, Days);
		return 1;
	}

	INT_PTR __cdecl SvcGetAllServerHistoryForContact(WPARAM hContact, LPARAM);
	INT_PTR __cdecl SvcGetAllServerHistory(WPARAM, LPARAM);
//=====================================================================================

	void CreateNewChat(LPCSTR uids, LPCTSTR ptrszTitle);
	__forceinline bool IsOnline() const { return m_bOnline; }
	void OnTimerTic();
	void ClearAccessToken();
	TCHAR* GetUserStoredPassword(void);
	void ShowCaptchaInBrowser(HBITMAP hBitmap);

	static mir_cs m_csTimer;
	static UINT_PTR m_timer;
	CVKOptions m_vkOptions;

private:

	friend struct AsyncHttpRequest;

//==== Enums =========================================================================

	enum CLMenuIndexes {
		CMI_VISITPROFILE,
		CMI_MARKMESSAGESASREAD,
		CMI_WALLPOST,
		CMI_ADDASFRIEND,
		CMI_DELETEFRIEND,
		CMI_BANUSER,
		CMI_REPORTABUSE,
		CMI_DESTROYKICKCHAT,
		CMI_OPENBROADCAST,
		CMI_LOADVKNEWS,
		CMI_GETSERVERHISTORY,
		CMI_COUNT
	};

	enum CLMenuHistoruIndexes {
		CHMI_GETSERVERHISTORY,
		CHMI_GETSERVERHISTORYLAST1DAY,
		CHMI_GETSERVERHISTORYLAST3DAY,
		CHMI_GETSERVERHISTORYLAST7DAY,
		CHMI_GETSERVERHISTORYLAST30DAY,
		CHMI_GETSERVERHISTORYLAST90DAY,
		CHMI_GETALLSERVERHISTORYFORCONTACT,
		CHMI_COUNT
	};

	enum ProtoMenuIndexes {
		PMI_CREATECHAT,
		PMI_SETSTATUSMSG,
		PMI_WALLPOST,
		PMI_LOADVKNEWS,
		PMI_GETALLSERVERHISTORY,
		PMI_WIPENONFRIENDS,
		PMI_VISITPROFILE,
		PMI_COUNT
	};

//====================================================================================	

	bool 
		m_prevError,
		m_bOnline,
		m_bNeedSendOnline,
		m_bSetBroadcast,
		m_bNotifyForEndLoadingHistory,
		m_bNotifyForEndLoadingHistoryAllContact, 
		m_bTerminated, 
		m_bServerDelivery;

	LONG m_myUserId;
	
	ptrA 
		m_pollingServer,
		m_pollingKey,
		m_pollingTs,
		m_szAccessToken;

	HANDLE
		m_pollingConn,
		m_hPollingThread,
		m_hAPIConnection,
		m_hPopupClassError,
		m_hPopupClassNotification,
		m_evRequestsQueue,
		m_hWorkerThread;

	ULONG m_msgId;

	mir_cs 
		m_csChatTyping,
		m_csLoadHistoryTask,
		m_csRequestsQueue,
		m_csSetStatus;

	int m_iLoadHistoryTask;

	LIST<void> 
		m_sendIds,
		m_incIds;

	OBJLIST<CVkChatInfo> m_chats;
	OBJLIST<CVKChatContactTypingParam> m_ChatsTyping;
	OBJLIST<CVkCookie> m_cookies;
	LIST<AsyncHttpRequest> m_arRequestsQueue;

	CMStringA m_prevUrl;

	HGENMENU 
		m_hContactMenuItems[CMI_COUNT],
		m_hContactHistoryMenuItems[CHMI_COUNT],
		m_hProtoMenuItems[PMI_COUNT];

//==== Menus =========================================================================

	void InitMenus();
	void UnInitMenus();
	int __cdecl OnPreBuildContactMenu(WPARAM hContact, LPARAM);

//==== PopUps ========================================================================

	void InitPopups(void);
	void MsgPopup(MCONTACT hContact, const TCHAR *szMsg, const TCHAR *szTitle, bool err = false);

//==== Hooks =========================================================================

	int __cdecl OnProcessSrmmEvent(WPARAM, LPARAM);
	int __cdecl OnDbEventRead(WPARAM, LPARAM);
	int __cdecl OnDbSettingChanged(WPARAM, LPARAM);

//==== Search ========================================================================

	void __cdecl SearchBasicThread(void *id);
	void __cdecl SearchByMailThread(void *email);
	void __cdecl SearchThread(void *p);
	void FreeProtoShearchStruct(PROTOSEARCHBYNAME *pParam);
	void OnSearch(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnSearchByMail(NETLIBHTTPREQUEST*, AsyncHttpRequest*);

//==== Files Upload ==================================================================

	void __cdecl SendFileThread(void *p);
	void SendFileFiled(CVkFileUploadParam *fup, int ErrorCode);
	void OnReciveUploadServer(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReciveUpload(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReciveUploadFile(NETLIBHTTPREQUEST*, AsyncHttpRequest*);

//==== Feed ==========================================================================

	void AddFeedSpecialUser();
	void AddFeedEvent(CVKNewsItem& vkNewsItem);
	void AddCListEvent(bool bNews);
	CVkUserInfo* GetVkUserInfo(LONG iUserId, OBJLIST<CVkUserInfo> &vkUsers);
	void CreateVkUserInfoList(OBJLIST<CVkUserInfo> &vkUsers, const JSONNode &jnResponse);
	CVKNewsItem* GetVkNewsItem(const JSONNode &jnItem, OBJLIST<CVkUserInfo> &vkUsers, bool isRepost = false);
	CVKNewsItem* GetVkGroupInvates(const JSONNode &jnItem, OBJLIST<CVkUserInfo> &vkUsers);
	CVKNewsItem* GetVkNotificationsItem(const JSONNode &jnItem, OBJLIST<CVkUserInfo> &vkUsers);
	CMString GetVkFeedback(const JSONNode &jnFeedback, VKObjType vkFeedbackType, OBJLIST<CVkUserInfo> &vkUsers, CVkUserInfo *vkUser);
	CVKNewsItem* GetVkParent(const JSONNode &jnParent, VKObjType vkParentType, LPCTSTR ptszReplyText = NULL, LPCTSTR ptszReplyLink = NULL);
	void RetrieveUnreadNews(time_t tLastNewsTime);
	void OnReceiveUnreadNews(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void RetrieveUnreadNotifications(time_t tLastNotificationsTime);
	bool FilterNotification(CVKNewsItem *vkNotificationItem, bool& isCommented);
	void NotificationMarkAsViewed();
	void OnReceiveUnreadNotifications(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void RetrieveUnreadEvents();
	void NewsClearHistory();
	INT_PTR __cdecl SvcLoadVKNews(WPARAM, LPARAM);

//====================================================================================

	void SetServerStatus(int);
	void RetrieveUsersInfo(bool flag = false, bool bRepeat = false);
	void RetrieveStatusMsg(const CMString &StatusMsg);
	void RetrieveStatusMusic(const CMString &StatusMsg);
	void OnReceiveStatus(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReceiveStatusMsg(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	MCONTACT SetContactInfo(const JSONNode &jnItem, bool flag = false, bool self = false);
	void RetrieveMyInfo(void);
	void OnReceiveMyInfo(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void RetrieveUserInfo(LONG userId);
	void OnReceiveUserInfo(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void RetrieveFriends(bool bCleanNonFriendContacts = false);
	void OnReceiveFriends(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void MarkMessagesRead(const CMStringA &mids);
	void MarkMessagesRead(const MCONTACT hContact);
	void RetrieveMessagesByIds(const CMStringA &mids);
	void RetrieveUnreadMessages();
	void OnReceiveMessages(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReceiveDlgs(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnSendMessage(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void WallPost(MCONTACT hContact, TCHAR *ptszMsg, TCHAR *ptszUrl, bool bFriendsOnly);
	void GetServerHistoryLastNDay(MCONTACT hContact, int NDay);
	void GetServerHistory(MCONTACT hContact, int iOffset, int iCount, int iTime, int iLastMsgId, bool once = false);
	void OnReceiveHistoryMessages(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq);
	void GetHistoryDlg(MCONTACT hContact, int iLastMsg);
	void RetrievePollingInfo();
	void OnReceivePollingInfo(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void __cdecl PollingThread(void*);
	int PollServer();
	void PollUpdates(const JSONNode&);
	void OnReceiveAuthRequest(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnReceiveDeleteFriend(NETLIBHTTPREQUEST*, AsyncHttpRequest*);

//==== Misc ==========================================================================

	void SetAllContactStatuses(int status);
	MCONTACT FindUser(LONG userid, bool bCreate = false);
	MCONTACT FindChat(LONG dwUserid);
	bool CheckMid(LIST<void> &lList, int guid);
	JSONNode& CheckJsonResponse(AsyncHttpRequest *pReq, NETLIBHTTPREQUEST *reply, JSONNode &root);
	bool CheckJsonResult(AsyncHttpRequest *pReq, const JSONNode &Node);
	void OnReceiveSmth(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	bool AutoFillForm(char*, CMStringA&, CMStringA&);
	CMString RunConfirmationCode();
	CMString RunRenameNick(LPCTSTR ptszOldName);
	void GrabCookies(NETLIBHTTPREQUEST *nhr);
	void ApplyCookies(AsyncHttpRequest*);
	void __cdecl DBAddAuthRequestThread(void *p);
	void DBAddAuthRequest(const MCONTACT hContact);
	MCONTACT MContactFromDbEvent(MEVENT hDbEvent);
	void SetMirVer(MCONTACT hContact, int platform);
	void __cdecl ContactTypingThread(void *p);
	void SetSrmmReadStatus(MCONTACT hContact);
	void MarkDialogAsRead(MCONTACT hContact);
	char* GetStickerId(const char *Msg, int& stickerid);
	CMString SpanVKNotificationType(CMString& tszType, VKObjType& vkFeedback, VKObjType& vkParent);
	CMString GetVkPhotoItem(const JSONNode &jnPhoto, BBCSupport iBBC);
	CMString SetBBCString(LPCTSTR tszString, BBCSupport iBBC, VKBBCType bbcType, LPCTSTR tszAddString = NULL);
	CMString& ClearFormatNick(CMString& tszText);
	CMString GetAttachmentDescr(const JSONNode &jnAttachments, BBCSupport iBBC = bbcNo);
	CMString GetFwdMessages(const JSONNode &jnMessages, const JSONNode &jnFUsers, BBCSupport iBBC = bbcNo);
	void SetInvisible(MCONTACT hContact);
	CMString RemoveBBC(CMString& tszSrc);
	void InitQueue();
	void UninitQueue();
	void ExecuteRequest(AsyncHttpRequest*);
	void __cdecl WorkerThread(void*);
	AsyncHttpRequest* Push(AsyncHttpRequest *pReq, int iTimeout = 10000);
	bool RunCaptchaForm(LPCSTR szUrl, CMStringA&);
	bool ApplyCaptcha(AsyncHttpRequest *pReq, const JSONNode&);
	void ConnectionFailed(int iReason);
	void OnLoggedIn();
	void OnLoggedOut();
	void ShutdownSession();
	void SetAvatarUrl(MCONTACT hContact, CMString &tszUrl);
	void GetAvatarFileName(MCONTACT hContact, TCHAR *pszDest, size_t cbLen);
	void ReloadAvatarInfo(MCONTACT hContact);
	void __cdecl SendMsgAck(void *param);
	void __cdecl ChatContactTypingThread(void *p);
	void StopChatContactTyping(int iChatId, int iUserId);
	void OnCreateNewChat(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	CVkChatInfo* AppendChat(int id, const JSONNode &jnNode);
	void SetChatTitle(CVkChatInfo *cc, LPCTSTR tszTopic);
	void AppendChatMessage(int id, const JSONNode &jnMsg, const JSONNode &jnFUsers, bool bIsHistory);
	void AppendChatMessage(CVkChatInfo *cc, int uid, int msgTime, LPCTSTR ptszBody, bool bIsHistory, bool bIsAction = false);
	void RetrieveChatInfo(CVkChatInfo*);
	void OnReceiveChatInfo(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnSendChatMsg(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	void OnChatLeave(NETLIBHTTPREQUEST*, AsyncHttpRequest *);
	void OnChatDestroy(NETLIBHTTPREQUEST*, AsyncHttpRequest*);
	int __cdecl OnChatEvent(WPARAM, LPARAM);
	int __cdecl OnGcMenuHook(WPARAM, LPARAM);
	void KickFromChat(int chat_id, int user_id, const JSONNode &jnMsg, const JSONNode &jnFUsers);
	void LeaveChat(int chat_id, bool close_window = true, bool delete_chat = false);
	INT_PTR __cdecl OnLeaveChat(WPARAM, LPARAM);
	INT_PTR __cdecl OnJoinChat(WPARAM, LPARAM);
	void LogMenuHook(CVkChatInfo*, GCHOOK*);
	void NickMenuHook(CVkChatInfo*, GCHOOK*);
	LPTSTR ChangeChatTopic(CVkChatInfo*);
	void SetChatStatus(MCONTACT hContact, int iStatus);
	CVkChatInfo* GetChatById(LPCTSTR ptszId);
	INT_PTR __cdecl SvcCreateChat(WPARAM, LPARAM);
};