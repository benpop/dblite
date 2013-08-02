#include "lua.h"
#include "lauxlib.h"

#include "sqlite3.h"

#include "dblite.h"


#define DBTYPE "DBLITE::DB"

#define MEMORY ":memory:"


#define toDbase(L) luaL_checkudata(L, 1, DBTYPE)

#define isClosed(ddb) ((ddb)->db == NULL)


static int error_from_code (lua_State *L, int code) {
  return luaL_error(L, sqlite3_errstr(code));
}


static Dbase *checkDb (lua_State *L) {
  Dbase *db = toDbase(L);
  if (isClosed(db))
    luaL_error(L, "operation on closed database");
  return db;
}


/*
** ALSO db_gc
*/
static int db_close (lua_State *L) {
  Dbase *db = toDbase(L);
  if (!isClosed(db)) {
    sqlite3 *sdb = db->db;
    db->db = NULL;
    sqlite3_close_v2(sdb);
  }
  return 0;
}


static int db_closed (lua_State *L) {
  Dbase *db = toDbase(L);
  lua_pushboolean(L, isClosed(db));
  return 1;
}


static int dblite_openname (lua_State *L, const char *name) {
  Dbase *db = lua_newuserdata(L, sizeof *db);
  int rc = sqlite3_open(name, &db->db);
  if (rc != SQLITE_OK) {
    sqlite3_close(db->db);
    return error_from_code(L, rc);
  }
  luaL_setmetatable(L, DBTYPE);
  return 1;
}


static int dblite_open (lua_State *L) {
  return dblite_openname(L, luaL_checkstring(L, 1));
}


static int dblite_openmemory (lua_State *L) {
  return dblite_openname(L, MEMORY);
}


static int dblite_call (lua_State *L) {
  luaL_checkstring(L, 2);
  lua_pushcfunction(L, dblite_open);
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  return 1;
}


static const luaL_Reg dblite[] = {
  {"open", dblite_open},
  {"open_memory", dblite_openmemory},
  {"__call", dblite_call},
  {NULL, NULL}
};


static const luaL_Reg db_meta[] = {
  {"__gc", db_close},
  {"close", db_close},
  {"closed", db_closed},
  {NULL, NULL}
};


int luaopen_dblite (lua_State *L) {
  luaL_newmetatable(L, DBTYPE);
  luaL_setfuncs(L, db_meta, 0);
  lua_pushvalue(L, -1);  /* copy metatable... */
  lua_setfield(L, -2, "__index");  /* ...and set it as its own index */
  luaL_newlib(L, dblite);
  lua_pushvalue(L, -1);  /* copy lib... */
  lua_setmetatable(L, -2);  /* ...and set it as its own metatable */
  return 1;
}

