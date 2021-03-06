#ifndef _LUA_SCRIPT_H_
#define _LUA_SCRIPT_H_

class CMLuaScript
{
public:
	lua_State *L;

	enum Status
	{
		None,
		Loaded,
		Failed
	};

private:
	int id;
	int unloadRef;
	char *moduleName;
	TCHAR *fileName;
	TCHAR filePath[MAX_PATH];
	Status status;

	static bool GetEnviroment(lua_State *L);

public:
	CMLuaScript(lua_State *L, const TCHAR *path);
	~CMLuaScript();

	static CMLuaScript* GetScriptFromEnviroment(lua_State *L);
	static int GetScriptIdFromEnviroment(lua_State *L);

	int GetId() const;

	const char* GetModuleName() const;

	const TCHAR* GetFilePath() const;
	const TCHAR* GetFileName() const;

	const Status GetStatus() const;

	bool Load();
	void Unload();
};

#endif //_LUA_SCRIPT_H_
