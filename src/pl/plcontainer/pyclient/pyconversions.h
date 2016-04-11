#ifndef PLC_PYCONVERSIONS_H
#define PLC_PYCONVERSIONS_H

#include <Python.h>
#include "common/messages/messages.h"

#define PLC_MAX_ARRAY_DIMS 10

typedef struct plcPyType plcPyType;
typedef PyObject *(*plcPyInputFunc)(char*);
typedef int (*plcPyOutputFunc)(PyObject*, char**, plcPyType*);

/* Working with arrays in Python */

typedef struct plcPyArrPointer {
    size_t    pos;
    PyObject *obj;
} plcPyArrPointer;

typedef struct plcPyArrMeta {
    int              ndims;
    size_t          *dims;
    plcPyType       *type;
    plcPyOutputFunc  outputfunc;
} plcPyArrMeta;

/* Working with types in Python */

typedef struct plcPyTypeConv {
    plcPyInputFunc  inputfunc;
    plcPyOutputFunc outputfunc;
} plcPyTypeConv;

typedef struct plcPyResult {
    plcontainer_result  res;
    plcPyTypeConv      *inconv;
} plcPyResult;

typedef struct plcPyType {
    plcDatatype    type;
    char          *name;
    int            nSubTypes;
    plcPyType     *subTypes;
    plcPyTypeConv  conv;
} plcPyType;

typedef struct plcPyFunction {
    plcProcSrc  proc;
    callreq     call;
    PyObject   *pyProc;
    int         nargs;
    plcPyType  *args;
    plcPyType   res;
    int         retset;
} plcPyFunction;

void plc_py_copy_type(plcType *type, plcPyType *pytype);

plcPyFunction *plc_py_init_function(callreq call);
plcPyResult  *plc_init_result_conversions(plcontainer_result res);
void plc_py_free_function(plcPyFunction *func);
void plc_free_result_conversions(plcPyResult *res);

#endif /* PLC_PYCONVERSIONS_H */