#ifndef _SCHEMAROUTER_H
#define _SCHEMAROUTER_H
/*
 * This file is distributed as part of the MariaDB Corporation MaxScale.  It is free
 * software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright MariaDB Corporation Ab 2013-2014
 */

/**
 * @file schemarouter.h - The schemarouter router module header file
 *
 * @verbatim
 * Revision History
 *
 * See GitHub https://github.com/mariadb-corporation/MaxScale
 *
 * @endverbatim
 */

#include <dcb.h>
#include <hashtable.h>
#include <mysql_client_server_protocol.h>

/**
 * Bitmask values for the router session's initialization. These values are used
 * to prevent responses from internal commands being forwarded to the client.
 */
typedef enum init_mask
{
  INIT_READY = 0x0,
  INIT_MAPPING = 0x1,
  INIT_USE_DB = 0x02,
  INIT_UNINT = 0x04

} init_mask_t;

/** 
 * The state of the backend server reference
 */
typedef enum bref_state {
        BREF_IN_USE           = 0x01,
        BREF_WAITING_RESULT   = 0x02, /*< for session commands only */
        BREF_QUERY_ACTIVE     = 0x04, /*< for other queries */
        BREF_CLOSED           = 0x08,
        BREF_DB_MAPPED           = 0x10
} bref_state_t;

#define BREF_IS_NOT_USED(s)         ((s)->bref_state & ~BREF_IN_USE)
#define BREF_IS_IN_USE(s)           ((s)->bref_state & BREF_IN_USE)
#define BREF_IS_WAITING_RESULT(s)   ((s)->bref_num_result_wait > 0)
#define BREF_IS_QUERY_ACTIVE(s)     ((s)->bref_state & BREF_QUERY_ACTIVE)
#define BREF_IS_CLOSED(s)           ((s)->bref_state & BREF_CLOSED)
#define BREF_IS_MAPPED(s)           ((s)->bref_mapped)

/** 
 * The type of the backend server
 */
typedef enum backend_type_t {
        BE_UNDEFINED=-1, 
        BE_MASTER, 
        BE_JOINED = BE_MASTER,
        BE_SLAVE,
        BE_COUNT
} backend_type_t;

struct router_instance;

/** 
 * Route target types
 */
typedef enum {
	TARGET_UNDEFINED    = 0x00,
	TARGET_MASTER       = 0x01,
	TARGET_SLAVE        = 0x02,
	TARGET_NAMED_SERVER = 0x04,
	TARGET_ALL          = 0x08,
	TARGET_RLAG_MAX     = 0x10,
	TARGET_ANY			= 0x20
} route_target_t;

#define TARGET_IS_UNDEFINED(t)	  (t == TARGET_UNDEFINED)
#define TARGET_IS_NAMED_SERVER(t) (t & TARGET_NAMED_SERVER)
#define TARGET_IS_ALL(t)          (t & TARGET_ALL)
#define TARGET_IS_ANY(t)          (t & TARGET_ANY)

typedef struct rses_property_st rses_property_t;
typedef struct router_client_session ROUTER_CLIENT_SES;

/** 
 * Router session properties
 */
typedef enum rses_property_type_t {
        RSES_PROP_TYPE_UNDEFINED=-1,
        RSES_PROP_TYPE_SESCMD=0,
        RSES_PROP_TYPE_FIRST = RSES_PROP_TYPE_SESCMD,
        RSES_PROP_TYPE_TMPTABLES,
        RSES_PROP_TYPE_LAST=RSES_PROP_TYPE_TMPTABLES,
	RSES_PROP_TYPE_COUNT=RSES_PROP_TYPE_LAST+1
} rses_property_type_t;

/** default values for rwsplit configuration parameters */
#define CONFIG_MAX_SLAVE_CONN 1
#define CONFIG_MAX_SLAVE_RLAG -1 /*< not used */
#define CONFIG_SQL_VARIABLES_IN TYPE_ALL

#define GET_SELECT_CRITERIA(s)                                                                  \
        (strncmp(s,"LEAST_GLOBAL_CONNECTIONS", strlen("LEAST_GLOBAL_CONNECTIONS")) == 0 ?       \
        LEAST_GLOBAL_CONNECTIONS : (                                                            \
        strncmp(s,"LEAST_BEHIND_MASTER", strlen("LEAST_BEHIND_MASTER")) == 0 ?                  \
        LEAST_BEHIND_MASTER : (                                                                 \
        strncmp(s,"LEAST_ROUTER_CONNECTIONS", strlen("LEAST_ROUTER_CONNECTIONS")) == 0 ?        \
        LEAST_ROUTER_CONNECTIONS : (                                                            \
        strncmp(s,"LEAST_CURRENT_OPERATIONS", strlen("LEAST_CURRENT_OPERATIONS")) == 0 ?        \
        LEAST_CURRENT_OPERATIONS : UNDEFINED_CRITERIA))))
        
/**
 * Session variable command
 */
typedef struct mysql_sescmd_st {
#if defined(SS_DEBUG)
        skygw_chk_t        my_sescmd_chk_top;
#endif
	rses_property_t*   my_sescmd_prop;       /*< Parent property */
        GWBUF*             my_sescmd_buf;        /*< Query buffer */
        unsigned char      my_sescmd_packet_type;/*< Packet type */
	bool               my_sescmd_is_replied; /*< Is cmd replied to client */
#if defined(SS_DEBUG)
        skygw_chk_t        my_sescmd_chk_tail;
#endif
} mysql_sescmd_t;


/**
 * Property structure
 */
struct rses_property_st {
#if defined(SS_DEBUG)
        skygw_chk_t          rses_prop_chk_top;
#endif
        ROUTER_CLIENT_SES*   rses_prop_rsession; /*< Parent router session */
        int                  rses_prop_refcount; /*< Reference count*/
        rses_property_type_t rses_prop_type; /*< Property type */
        union rses_prop_data {
                mysql_sescmd_t  sescmd; /*< Session commands */
		HASHTABLE*	temp_tables; /*< Hashtable of table names */
        } rses_prop_data;
        rses_property_t*     rses_prop_next; /*< Next property of same type */
#if defined(SS_DEBUG)
        skygw_chk_t          rses_prop_chk_tail;
#endif
};

typedef struct sescmd_cursor_st {
#if defined(SS_DEBUG)
        skygw_chk_t        scmd_cur_chk_top;
#endif
        ROUTER_CLIENT_SES* scmd_cur_rses;         /*< pointer to owning router session */
	rses_property_t**  scmd_cur_ptr_property; /*< address of pointer to owner property */
	mysql_sescmd_t*    scmd_cur_cmd;          /*< pointer to current session command */
	bool               scmd_cur_active;       /*< true if command is being executed */
#if defined(SS_DEBUG)
	skygw_chk_t        scmd_cur_chk_tail;
#endif
} sescmd_cursor_t;

/**
 * Internal structure used to define the set of backend servers we are routing
 * connections to. This provides the storage for routing module specific data
 * that is required for each of the backend servers.
 * 
 * Owned by router_instance, referenced by each routing session.
 */
typedef struct backend_st {
#if defined(SS_DEBUG)
        skygw_chk_t     be_chk_top;
#endif
        SERVER*         backend_server;      /*< The server itself */
        int             backend_conn_count;  /*< Number of connections to
					      *  the server
					      */
        bool            be_valid; 	     /*< Valid when belongs to the
					      *  router's configuration
					      */
	int		weight;		     /*< Desired weighting on the
					      *  load. Expressed in .1%
					      * increments
					      */
        struct stats{
          int queries;
        } stats;
#if defined(SS_DEBUG)
        skygw_chk_t     be_chk_tail;
#endif
} BACKEND;


/**
 * Reference to BACKEND.
 * 
 * Owned by router client session.
 */
typedef struct backend_ref_st {
#if defined(SS_DEBUG)
        skygw_chk_t     bref_chk_top;
#endif
        BACKEND*        bref_backend; /*< Backend server */
        DCB*            bref_dcb; /*< Backend DCB */
        bref_state_t    bref_state; /*< State of the backend */
        bool            bref_mapped; /*< Whether the backend has been mapped */
        int             bref_num_result_wait; /*< Number of not yet received results */
        sescmd_cursor_t bref_sescmd_cur; /*< Session command cursor */
	GWBUF*          bref_pending_cmd; /*< For stmt which can't be routed due active sescmd execution */
#if defined(SS_DEBUG)
        skygw_chk_t     bref_chk_tail;
#endif
} backend_ref_t;

/**
 * Configuration values
 */
typedef struct schemarouter_config_st {
        int               rw_max_slave_conn_percent;
        int               rw_max_slave_conn_count;
	target_t          rw_use_sql_variables_in;	
} schemarouter_config_t;
     

/**
 * The client session structure used within this router.
 */
struct router_client_session {
#if defined(SS_DEBUG)
        skygw_chk_t      rses_chk_top;
#endif
        SPINLOCK         rses_lock;      /*< protects rses_deleted                 */
        int              rses_versno;    /*< even = no active update, else odd. not used 4/14 */
        bool             rses_closed;    /*< true when closeSession is called      */
	    DCB*			 rses_client_dcb;
	    MYSQL_session*   rses_mysql_session; /*< Session client data (username, password, SHA1). */
	/** Properties listed by their type */
	rses_property_t* rses_properties[RSES_PROP_TYPE_COUNT]; /*< Session properties */
        backend_ref_t*   rses_master_ref; /*< Router session master reference */
        backend_ref_t*   rses_backend_ref; /*< Pointer to backend reference array */
        schemarouter_config_t rses_config;    /*< Copied config info from router instance */
        int              rses_nbackends; /*< Number of backends */
        int              rses_capabilities; /*< Input type, for example */
        bool             rses_autocommit_enabled; /*< Is autocommit enabled */
        bool             rses_transaction_active; /*< Is a transaction active */
	struct router_instance	 *router;	/*< The router instance */
        struct router_client_session* next; /*< List of router sessions */
        HASHTABLE*      dbhash; /*< Database hash containing names of the databases mapped to the servers that contain them */
        char            connect_db[MYSQL_DATABASE_MAXLEN+1]; /*< Database the user was trying to connect to */
        init_mask_t    init; /*< Initialization state bitmask */
        GWBUF*          queue; /*< Query that was received before the session was ready */
        DCB*            dcb_route; /*< Internal DCB used to trigger re-routing of buffers */
        DCB*            dcb_reply; /*< Internal DCB used to send replies to the client */
#if defined(SS_DEBUG)
        skygw_chk_t      rses_chk_tail;
#endif
};

/**
 * The statistics for this router instance
 */
typedef struct {
	int		n_sessions;	/*< Number sessions created        */
	int		n_queries;	/*< Number of queries forwarded    */
	int		n_master;	/*< Number of stmts sent to master */
	int		n_slave;	/*< Number of stmts sent to slave  */
	int		n_all;		/*< Number of stmts sent to all    */
} ROUTER_STATS;


/**
 * The per instance data for the router.
 */
typedef struct router_instance {
	SERVICE*                service;     /*< Pointer to service                 */
	ROUTER_CLIENT_SES*      connections; /*< List of client connections         */
	SPINLOCK                lock;	     /*< Lock for the instance data         */
	BACKEND**               servers;     /*< Backend servers                    */
	BACKEND*                master;      /*< NULL or pointer                    */
	schemarouter_config_t        schemarouter_config; /*< expanded config info from SERVICE */
	int                     schemarouter_version;/*< version number for router's config */
        unsigned int	        bitmask;     /*< Bitmask to apply to server->status */
	unsigned int	        bitvalue;    /*< Required value of server->status   */
	ROUTER_STATS            stats;       /*< Statistics for this router         */
        struct router_instance* next;        /*< Next router on the list            */
	bool			available_slaves; /*< The router has some slaves available */
	//HASHTABLE* dbnames_hash; /** Hashtable containing the database names and where to find them */
	//char** ignore_list;
} ROUTER_INSTANCE;

#define BACKEND_TYPE(b) (SERVER_IS_MASTER((b)->backend_server) ? BE_MASTER :    \
        (SERVER_IS_SLAVE((b)->backend_server) ? BE_SLAVE :  BE_UNDEFINED));
#if 0
void* dbnames_hash_init(ROUTER_INSTANCE* inst,BACKEND** backends);
bool update_dbnames_hash(ROUTER_INSTANCE* inst,BACKEND** backends, HASHTABLE* hashtable);
#endif

#endif /*< _SCHEMAROUTER_H */
