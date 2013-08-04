#include <ctype.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "sqlite3.h"


#define DB_META "DBLITE::DB"
#define STMT_META "DBLITE::STMT"

#define MEMORY ":memory:"


typedef struct Dbase {
  sqlite3 *db;
} Dbase;


typedef struct Stmt {
  sqlite3_stmt *stmt;
} Stmt;


/* ====================================================== */


static int error_from_code (lua_State *L, int code) {
  return luaL_error(L, "(%d) %s", code, sqlite3_errstr(code));
}


/* ====================================================== */


#define toStmt(L) luaL_checkudata(L, 1, STMT_META)

#define isFinalized(sstmt) ((sstmt)->stmt == NULL)


static Stmt *checkStmt (lua_State *L) {
  Stmt *stmt = toStmt(L);
  if (isFinalized(stmt))
    luaL_error(L, "operation on finalized statement");
  return stmt;
}


static int stmt_gc (lua_State *L) {
  Stmt *stmt = toStmt(L);
  lua_pushnil(L);
  lua_rawsetp(L, LUA_REGISTRYINDEX, stmt);  /* remove reference to DB */
  if (!isFinalized(stmt)) {
    sqlite3_stmt *sstmt = stmt->stmt;
    stmt->stmt = NULL;
    sqlite3_finalize(sstmt);
  }
  return 0;
}


static int stmt_tostring (lua_State *L) {
  Stmt *stmt = toStmt(L);
  if (isFinalized(stmt))
    lua_pushliteral(L, "sqlite3 statement (closed)");
  else
    lua_pushfstring(L, "sqlite3 statement (%p)", stmt);
  return 1;
}


static int stmt_sql (lua_State *L) {
  Stmt *stmt = checkStmt(L);
  lua_pushstring(L, sqlite3_sql(stmt->stmt));
  return 1;
}


/* ====================================================== */


#define toDbase(L) luaL_checkudata(L, 1, DB_META)

#define isClosed(ddb) ((ddb)->db == NULL)


static Dbase *checkDbase (lua_State *L) {
  Dbase *db = toDbase(L);
  if (isClosed(db))
    luaL_error(L, "operation on closed database");
  return db;
}


static void db_close_aux (Dbase *db) {
  if (!isClosed(db)) {
    sqlite3 *sdb = db->db;
    db->db = NULL;
    sqlite3_close_v2(sdb);
  }
}


static int db_close (lua_State *L) {
  Dbase *db = toDbase(L);
  db_close_aux(db);
  return 0;
}


static int db_gc (lua_State *L) {
  Dbase *db = toDbase(L);
  lua_pushnil(L);
  lua_rawsetp(L, LUA_REGISTRYINDEX, db);  /* remove reference to name */
  db_close_aux(db);
  return 0;
}


static int db_isclosed (lua_State *L) {
  Dbase *db = toDbase(L);
  lua_pushboolean(L, isClosed(db));
  return 1;
}


static int db_name (lua_State *L) {
  Dbase *db = toDbase(L);
  lua_rawgetp(L, LUA_REGISTRYINDEX, db);
  return 1;
}


static int db_tostring (lua_State *L) {
  Dbase *db = toDbase(L);
  const char *address = "closed";
  const char *name;
  lua_rawgetp(L, LUA_REGISTRYINDEX, db);  /* get name */
  name = lua_tostring(L, -1);
  if (!isClosed(db))
    address = lua_pushfstring(L, "%p", db->db);
  lua_pushfstring(L, "sqlite3 database \"%s\" (%s)", name, address);
  return 1;
}


/* == DB EXECUTE SQL ==================================== */


static int callable (lua_State *L, int index) {
  switch (lua_type(L, index)) {
    case LUA_TFUNCTION: return 1;
    case LUA_TTABLE:  case LUA_TUSERDATA: {
      if (luaL_getmetafield(L, index, "__call")) {
        lua_pop(L, 1);
        return 1;
      }
      /* fall-through */
    }
    default: return 0;
  }
}


#define checkcallable(L,i) do {                         \
    if (!callable(L, (i))) {                            \
      luaL_argerror(L, (i),                             \
        lua_pushfstring(L, "callable expected, got %s", \
          luaL_typename(L, (i))));                      \
    }                                                   \
  } while (0)


static void build_row (lua_State *L, int rowidx, int n, char **cols) {
  int i;
  lua_createtable(L, n, 0);
  lua_replace(L, rowidx);
  for (i = 0; i < n; i++) {
    lua_pushstring(L, cols[i]);
    lua_rawseti(L, rowidx, i + 1);
  }
}


/*
** Stack slots:
** 3: callback
** 4: user value
** 5: # of columns
** 6: column data
** 7: column names
*/
static int db_exec_callback (void *p, int n, char **coldata, char **colnames) {
  lua_State *L = p;
  if (!lua_isnumber(L, 5)) {  /* # of columns */
    lua_pushinteger(L, n);
    lua_replace(L, 5);
  }
  build_row(L, 6, n, coldata);  /* column data - refresh for each row */
  if (!lua_istable(L, 7))  /* column names */
    build_row(L, 7, n, colnames);
  lua_pushvalue(L, 3);  /* callback */
  lua_pushvalue(L, 4);  /* user value */
  lua_pushvalue(L, 5);  /* # of columns */
  lua_pushvalue(L, 6);  /* column data */
  lua_pushvalue(L, 7);  /* column names */
  return lua_pcall(L, 4, 0, 0) == LUA_OK ? SQLITE_OK : SQLITE_ABORT;
}


/*
** db:execute(sql [, callback [, user value]])
**
** callback(user value, n, data, names)
** n: number of columns
** data: column data
** names: column names (static throughout calls)
*/
static int db_exec (lua_State *L) {
  Dbase *db = checkDbase(L);
  const char *sql = luaL_checkstring(L, 2);
  int rc;
  if (!lua_isnoneornil(L, 3)) {
    checkcallable(L, 3);
    lua_settop(L, 4);  /* truncate excess values */
    lua_settop(L, 7);  /* expand for callback sub-stack */
    rc = sqlite3_exec(db->db, sql, db_exec_callback, L, NULL);
  }
  else
    rc = sqlite3_exec(db->db, sql, NULL, NULL, NULL);
  return rc == SQLITE_OK ? 0 : error_from_code(L, rc);
}


/* == DB PREPARE STATEMENT ============================== */


static int db_prepare (lua_State *L) {
  Dbase *db = checkDbase(L);
  size_t nsql;
  const char *sql = luaL_checklstring(L, 2, &nsql);
  const char *tail;
  Stmt *stmt = lua_newuserdata(L, sizeof *stmt);
  int rc = sqlite3_prepare_v2(db->db, sql, (int)nsql, &stmt->stmt, &tail);
  if (rc != SQLITE_OK) return error_from_code(L, rc);
  luaL_setmetatable(L, STMT_META);
  lua_pushvalue(L, 1);  /* copy DB */
  lua_rawsetp(L, LUA_REGISTRYINDEX, stmt);  /* save DB under address */
  if (tail < sql + nsql) {  /* unread statements remaining */
    while (isspace(*tail)) tail++;  /* trim leading space */
    if (*tail) {
      lua_pushstring(L, tail);
      return 2;
    }
  }
  return 1;
}


/* ====================================================== */


static int dblite_openname (lua_State *L, const char *name) {
  Dbase *db = lua_newuserdata(L, sizeof *db);
  int rc = sqlite3_open(name, &db->db);
  if (rc != SQLITE_OK) {
    sqlite3_close(db->db);
    return error_from_code(L, rc);
  }
  luaL_setmetatable(L, DB_META);
  /* save name */
  lua_pushstring(L, name);
  lua_rawsetp(L, LUA_REGISTRYINDEX, db);
  /* turn on extended result codes */
  sqlite3_extended_result_codes(db->db, 1);
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


/* ====================================================== */


static const luaL_Reg dblite[] = {
  {"open", dblite_open},
  {"open_memory", dblite_openmemory},
  {"__call", dblite_call},
  {NULL, NULL}
};


static const luaL_Reg db_meta[] = {
  {"__gc", db_gc},
  {"__tostring", db_tostring},
  {"close", db_close},
  {"isclosed", db_isclosed},
  {"name", db_name},
  {"execute", db_exec},
  {"prepare", db_prepare},
  {NULL, NULL}
};


static const luaL_Reg stmt_meta[] = {
  {"__gc", stmt_gc},
  {"__tostring", stmt_tostring},
  {"sql", stmt_sql},
  {NULL, NULL}
};


static void push_metatable (lua_State *L, const char *tname,
                            const luaL_Reg *funcs) {
  luaL_newmetatable(L, tname);
  luaL_setfuncs(L, funcs, 0);
  lua_pushvalue(L, -1);  /* copy metatable... */
  lua_setfield(L, -2, "__index");  /* ...and set it as its own index */
}


int luaopen_dblite (lua_State *L) {
  push_metatable(L, DB_META, db_meta);
  push_metatable(L, STMT_META, stmt_meta);
  luaL_newlib(L, dblite);
  lua_pushvalue(L, -1);  /* copy lib... */
  lua_setmetatable(L, -2);  /* ...and set it as its own metatable */
  return 1;
}

