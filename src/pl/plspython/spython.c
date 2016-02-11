/* a POC to implement pl that uses containers and is language agnostic */

#include "postgres.h"

#include "common/comm_channel.h"
#include "common/messages/messages.h"
#include "fmgr.h"
#include "message_fns.h"

#include "executor/spi_priv.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "sqlhandler.h"
#include "utils/portal.h"

#include "access/xact.h"
#include "commands/trigger.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include <string.h>

#include "containers.h"

#include "postgres.h"

#include "logging.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(plspython_call_handler);

/* entrypoint for all plspython procedures */
Datum plspython_call_handler(PG_FUNCTION_ARGS);

static void plspython_exception_do(error_message);
static void plspython_sql_do(sql_msg msg);
static void  plspython_log_do(log_message);
static Datum plspython_call_hook(PG_FUNCTION_ARGS);

Datum plspython_call_handler(PG_FUNCTION_ARGS) {
    Datum datumreturn;
    int ret;

    ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        elog(ERROR, "[pl-j core] SPI connect error: %d (%s)", ret,
             SPI_result_code_string(ret));

    plelog("Before plspython_call_hook()");
    datumreturn = plspython_call_hook(fcinfo);
    plelog("After plspython_call_hook()");

    ret = SPI_finish();
    if (ret != SPI_OK_FINISH)
        elog(ERROR, "SPI finis error: %d (%s)", ret,
             SPI_result_code_string(ret));

    //elog(DEBUG1, "SPI_finish called");
    return datumreturn;
}

static Datum plspython_call_hook(PG_FUNCTION_ARGS) {
    char *      name;
    callreq     req;
    int         message_type;
    PGconn_min *conn;
    proc_info   pinfo;

    plelog("Inside plspython_call_hook()");

    /* TODO: handle trigger requests as well */
    if (CALLED_AS_TRIGGER(fcinfo)) {
        elog(ERROR, "triggers aren't supported");
    }

    fill_proc_info(fcinfo, &pinfo);
    req = plspython_create_call(fcinfo, &pinfo);

    name = parse_container_name(req->proc.src);
    conn = find_container(name);
    if (conn == NULL) {
        plelog("Before start_container()");
        conn = start_container(name);
        plelog("After start_container()");
    }

    plelog("Before plspython_channel_initialize()");
    plspython_channel_initialize(conn);
    plelog("After plspython_channel_initialize()");

    plelog("Before plspython_channel_send()");
    plspython_channel_send((message)req);
    plelog("After plspython_channel_send()");

    free(name);
    do {
        message       ansver = NULL;
        MemoryContext messageContext;
        MemoryContext oldContext;

        messageContext = AllocSetContextCreate(
            CurrentMemoryContext, "PL message context", 128000, 32, 65536);
        oldContext = CurrentMemoryContext;

        MemoryContextSwitchTo(messageContext);

        plelog("Before plspython_channel_receive()");
        /* pljelog(DEBUG1, "waiting ansver."); */
        ansver = plspython_channel_receive();
        plelog("After plspython_channel_receive()");

        elog(DEBUG1, "received message: %c", ((message)ansver)->msgtype);
        message_type = ansver->msgtype;
        /* pljelog(DEBUG1, "ansver of type: %d", message_type); */
        switch (message_type) {
        case MT_RESULT:
            // handle elsewhere
            break;
        case MT_EXCEPTION:
            plspython_exception_do((error_message)ansver);
            PG_RETURN_NULL();
            break;
        case MT_SQL:
            plspython_sql_do((sql_msg)ansver);
            break;
        case MT_LOG:
            plspython_log_do((log_message)ansver);
            break;
        case MT_TUPLRES:
            break;
        default:
            // lets first switch back to the old context and handle the error.
            MemoryContextSwitchTo(oldContext);
            MemoryContextDelete(messageContext);
            /* pljelog(FATAL, "[plj core] received: unhandled message with type
             * id %d", message_type); */
            // this wont run.
            PG_RETURN_NULL();
        }

        // we do not try to free messages anymore, we switch context and delete
        // the old one
        // elog(DEBUG1, "[plj core] free message");
        // elog(DEBUG1, "[plj core] free message done");

        /*
         * here is how to escape from the loop
         */
        /* pljelog(DEBUG1, "begin return procedure"); */

        if (message_type == MT_RESULT) {
            plelog("Handling result message");
            /* pljelog(DEBUG1, "result"); */
            plspython_result res = (plspython_result)ansver;

            /* pljelog(DEBUG1, "result 1"); */
            if (res->rows == 1 && res->cols == 1) {
                Datum        ret;
                HeapTuple    typetup;
                Form_pg_type type;
                Oid          typeOid;
                Datum        rawDatum;
                int32        typeMod;
                /* MemoryContext oldctx; */

                plelog("One row one column");

                if (res->data[0][0].isnull == 1) {
                    plelog("Null received");
                    MemoryContextSwitchTo(oldContext);
                    MemoryContextDelete(messageContext);
                    PG_RETURN_NULL();
                }

                /* pljelog(DEBUG1, "1"); */
                plelog("Before parseTypeString()");
                //elog(DEBUG1, "Type string: '%s'", res->types[0]);
                parseTypeString(res->types[0], &typeOid, &typeMod);
                plelog("After parseTypeString()");
                plelog("Before SearchSysCache()");
                typetup = SearchSysCache(TYPEOID, typeOid, 0, 0, 0);
                plelog("After SearchSysCache()");
                if (!HeapTupleIsValid(typetup)) {
                    MemoryContextSwitchTo(oldContext);
                    MemoryContextDelete(messageContext);
                    elog(FATAL,
                         "[plj - core] Invalid heaptuple at result return");
                    // This won`t run
                    PG_RETURN_NULL();
                }

                type = (Form_pg_type)GETSTRUCT(typetup);

                /* TODO: we don't need that since SPI_palloc allocates memory
                 * from the upper executor context
                 */
                /* oldctx = CurrentMemoryContext; */
                /* MemoryContextSwitchTo(QueryContext); */

                rawDatum = CStringGetDatum(res->data[0][0].value);
                ret      = OidFunctionCall1(type->typinput, rawDatum);
                ReleaseSysCache(typetup);

                /* MemoryContextSwitchTo(oldctx); */

                return ret;
            } else if (res->rows == 0) {
                MemoryContextSwitchTo(oldContext);
                MemoryContextDelete(messageContext);
                /* pljelog(ERROR, "Resultset return not implemented."); */
                PG_RETURN_VOID();
            }

            /*
             * continue here!!
             */

            /*			PG_RETURN_NULL(); */
            /*
             * break;
             */
        }

        //elog(DEBUG1, "deleting message ctx");
        MemoryContextSwitchTo(oldContext);
        MemoryContextDelete(messageContext);
        //elog(DEBUG1, "deleted message ctx");

        if (message_type == MT_EXCEPTION) {
            elog(ERROR, "Exception caught: %s", ((error_message)ansver)->message);
            PG_RETURN_NULL();
        }
        //elog(DEBUG1, "debug");

        /* pljelog(ERROR, "no handler for message type: %d", message_type); */

    } while (1);

    /* pljelog(DEBUG1, "return null"); */
    PG_RETURN_NULL();
}

void
plspython_log_do(log_message log) {
    int level = DEBUG1;

    if (log == NULL)
        return;
    switch (log->level) {
    case 1:
        level = DEBUG5;
        break;
    case 2:
        level = INFO;
        break;
    default:
        level = INFO;
    }

    elog(level, "[%s] -  %s ", log->category, log->message);
}

void
plspython_sql_do(sql_msg msg) {
    message res;
    res = handle_sql_message(msg);
    if (res != NULL)
        plspython_channel_send((message)res);
}

void
plspython_exception_do(error_message msg) {
    elog(ERROR, "exception occured: \n %s \n ", msg->message);
}
