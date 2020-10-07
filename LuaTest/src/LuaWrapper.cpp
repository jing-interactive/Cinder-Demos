#include "LuaWrapper.h"

#include "../../blocks/lua/src/lua.hpp"

#include <cinder/Log.h>

bool LuaWrapper::setup(const char* filename)
{
    L = luaL_newstate();
    luaL_openlibs(L);
    int ret = luaL_dostring(L, filename);

    if (ret != LUA_OK) {
        CI_LOG_E(lua_tostring(L, -1));
        lua_pop(L, 1); // pop error message
        return false;
    }

    return true;
}

void LuaWrapper::cleanup()
{
    lua_close(L);
}