#ifndef dblite_h
#define dblite_h

#define DBLITE_BOTH     0
#define DBLITE_NUMERIC  1
#define DBLITE_NAMED    2

#define is_numeric(t)   ((t) == DBLITE_BOTH || (t) == DBLITE_NUMERIC)
#define is_named(t)     ((t) == DBLITE_BOTH || (t) == DBLITE_NAMED)

typedef struct Db {
  struct sqlite3 *db;
} Db;

#endif
