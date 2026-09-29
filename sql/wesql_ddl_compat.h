#ifndef WESQL_DDL_COMPAT_H
#define WESQL_DDL_COMPAT_H

/*
  SmartEngine CREATE/ALTER compatibility on long-lived 8.0 (MySQL 8.0.46).

  After parse, before the storage engine: remap unsupported index collations
  and strip FOREIGN KEY from SmartEngine tables. InnoDB is unchanged.
  Rewrites parsed HA_CREATE_INFO / Alter_info, not SQL text.
  Not GitHub PR #85. Not the 9.7 tree.
*/

#include "lex_string.h"

class THD;
class Alter_info;
struct HA_CREATE_INFO;

/*
  serverless_honor_innodb_engine. In serverless mode an explicit
  ENGINE=<other engine> on a user table is replaced by SMARTENGINE. When this
  is ON, an explicit ENGINE=InnoDB is kept, so the table stays on InnoDB and
  keeps its foreign keys. Default OFF.
*/
extern bool opt_serverless_honor_innodb_engine;

/*
  Whether a user table created or altered with an explicit ENGINE=<engine>
  keeps that engine in serverless mode instead of being moved to SMARTENGINE.
*/
bool wesql_serverless_keeps_engine(const LEX_CSTRING &engine);

/*
  Write an error-log warning when an explicit ENGINE=<engine> was replaced by
  SMARTENGINE. The client already gets ER_FORCE_STORAGE_ENGINE_TO_SMARTENGINE.
*/
void wesql_log_engine_substitution(THD *thd, const LEX_CSTRING &engine);

bool wesql_is_smartengine_create(const HA_CREATE_INFO *create_info);

/* CREATE: after set_table_default_charset(), before mysql_prepare_create_table(). */
bool wesql_ddl_compat_rewrite_create(THD *thd, HA_CREATE_INFO *create_info,
                                     Alter_info *alter_info);

/*
  ALTER: after the engine is known, before check_fk_parent_table_access()
  and FK MDL collection.
*/
bool wesql_ddl_compat_rewrite_alter_fk(THD *thd, HA_CREATE_INFO *create_info,
                                       Alter_info *alter_info);

/* ALTER: after mysql_prepare_alter_table() and set_table_default_charset(). */
bool wesql_ddl_compat_rewrite_alter_collation(THD *thd,
                                              HA_CREATE_INFO *create_info,
                                              Alter_info *alter_info);

#endif
