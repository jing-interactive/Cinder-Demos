#pragma once

struct LuaWrapper
{
    struct lua_State* L;

    bool setup(const char* filename);
    void cleanup();
};
