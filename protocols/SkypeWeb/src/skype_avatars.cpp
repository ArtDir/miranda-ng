/*
Copyright (c) 2015-16 Miranda NG project (http://miranda-ng.org)

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

INT_PTR CSkypeProto::SvcGetAvatarCaps(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case AF_MAXSIZE:
		((POINT*)lParam)->x = 98;
		((POINT*)lParam)->y = 98;
		return 0;

	case AF_MAXFILESIZE:
		return 32000;

	case AF_PROPORTION:
		return PIP_SQUARE;

	case AF_FORMATSUPPORTED:
	case AF_ENABLED:
	case AF_DONTNEEDDELAYS:
	case AF_FETCHIFPROTONOTVISIBLE:
	case AF_FETCHIFCONTACTOFFLINE:
		return 1;
	}
	return 0;
}

void CSkypeProto::ReloadAvatarInfo(MCONTACT hContact)
{
	if (hContact == NULL)
	{
		CallService(MS_AV_REPORTMYAVATARCHANGED, (WPARAM)m_szModuleName, 0);
		return;
	}
	PROTO_AVATAR_INFORMATION ai = { 0 };
	ai.hContact = hContact;
	SvcGetAvatarInfo(0, (LPARAM)&ai);
}

void CSkypeProto::OnReceiveAvatar(const NETLIBHTTPREQUEST *response, void *arg)
{
	if (response == NULL || response->pData == NULL)
		return;
	
	MCONTACT hContact = (DWORD_PTR)arg;
	if (response->resultCode != 200)
		return;

	PROTO_AVATAR_INFORMATION ai = { 0 };
	ai.format = ProtoGetBufferFormat(response->pData);
	setByte(hContact, "AvatarType", ai.format);
	GetAvatarFileName(hContact, ai.filename, _countof(ai.filename));

	FILE *out = _tfopen(ai.filename, _T("wb"));
	if (out == NULL) {
		ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, &ai, 0);
		return;
	}

	fwrite(response->pData, 1, response->dataLength, out);
	fclose(out);
	setByte(hContact, "NeedNewAvatar", 0);
	ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, &ai, 0);
}

void CSkypeProto::OnSentAvatar(const NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		return;

	JSONNode root = JSONNode::parse(response->pData);
	if (!root)
		return;
}

INT_PTR CSkypeProto::SvcGetAvatarInfo(WPARAM, LPARAM lParam)
{
	PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION*)lParam;

	ptrA szUrl(getStringA(pai->hContact, "AvatarUrl"));
	if (szUrl == NULL)
		return GAIR_NOAVATAR;

	pai->format = getByte(pai->hContact, "AvatarType", PA_FORMAT_JPEG);

	TCHAR tszFileName[MAX_PATH];
	GetAvatarFileName(pai->hContact, tszFileName, _countof(tszFileName));
	_tcsncpy(pai->filename, tszFileName, _countof(pai->filename));

	if (::_taccess(pai->filename, 0) == 0 && !getBool(pai->hContact, "NeedNewAvatar", 0))
		return GAIR_SUCCESS;

	if (IsOnline()) {
		PushRequest(new GetAvatarRequest(szUrl), &CSkypeProto::OnReceiveAvatar, (void*)pai->hContact);
		debugLogA("Requested to read an avatar from '%s'", szUrl);
		return GAIR_WAITFOR;
	}

	debugLogA("No avatar");
	return GAIR_NOAVATAR;
}

INT_PTR CSkypeProto::SvcGetMyAvatar(WPARAM wParam, LPARAM lParam)
{
	TCHAR path[MAX_PATH];
	GetAvatarFileName(NULL, path, _countof(path));
	_tcsncpy((TCHAR*)wParam, path, (int)lParam);
	return 0;
}

void CSkypeProto::GetAvatarFileName(MCONTACT hContact, TCHAR* pszDest, size_t cbLen)
{
	int tPathLen = mir_sntprintf(pszDest, cbLen, _T("%s\\%s"), VARST(_T("%miranda_avatarcache%")), m_tszUserName);

	DWORD dwAttributes = GetFileAttributes(pszDest);
	if (dwAttributes == 0xffffffff || (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		CreateDirectoryTreeT(pszDest);

	pszDest[tPathLen++] = '\\';

	const TCHAR* szFileType = ProtoGetAvatarExtension(getByte(hContact, "AvatarType", PA_FORMAT_JPEG));
	CMStringA username(Contacts[hContact]);
	username.Replace("live:", "__live_");
	username.Replace("facebook:", "__facebook_");
	mir_sntprintf(pszDest + tPathLen, MAX_PATH - tPathLen, _T("%S%s"), username.c_str(), szFileType);
}

void CSkypeProto::SetAvatarUrl(MCONTACT hContact, CMString &tszUrl)
{
	ptrT oldUrl(getTStringA(hContact, "AvatarUrl"));
	if (oldUrl != NULL)
		if (tszUrl == oldUrl)
			return;

	if (tszUrl.IsEmpty()) {
		delSetting(hContact, "AvatarUrl");
		ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, NULL, 0);
	}
	else {
		setTString(hContact, "AvatarUrl", tszUrl.GetBuffer());
		setByte(hContact, "NeedNewAvatar", 1);
		PROTO_AVATAR_INFORMATION ai = { 0 };
		ai.hContact = hContact;
		GetAvatarFileName(ai.hContact, ai.filename, _countof(ai.filename));
		ai.format = ProtoGetAvatarFormat(ai.filename);
		ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, 0);
	}
}

INT_PTR CSkypeProto::SvcSetMyAvatar(WPARAM, LPARAM lParam)
{
	TCHAR *path = (TCHAR*)lParam;
	TCHAR avatarPath[MAX_PATH];
	GetAvatarFileName(NULL, avatarPath, _countof(avatarPath));
	if (path != NULL)
	{
		if (CopyFile(path, avatarPath, FALSE))
		{
			FILE *hFile = _tfopen(path, L"rb");
			if (hFile)
			{
				fseek(hFile, 0, SEEK_END);
				size_t length = ftell(hFile);
				if (length != -1)
				{
					rewind(hFile);

					mir_ptr<BYTE> data((PBYTE)mir_alloc(length));

					if (data != NULL && fread(data, sizeof(BYTE), length, hFile) == length)
					{
						const char *szMime = "image/jpeg";
						if (fii)
							szMime = fii->FI_GetFIFMimeType(fii->FI_GetFIFFromFilenameU(path));

						PushRequest(new SetAvatarRequest(data, length, szMime, li), &CSkypeProto::OnSentAvatar);
						fclose(hFile);
						return 0;
					}
				}
				fclose(hFile);
			}
		}
		return -1;
	}
	else
	{
		if (IsFileExists(avatarPath))
		{
			DeleteFile(avatarPath);
		}
	}

	return 0;
}
