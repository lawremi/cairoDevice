#include <R_ext/Rdynload.h>
#include <Rinternals.h>

SEXP do_asCairoDevice(SEXP widget, SEXP pointsize, SEXP width, SEXP height);

#define CALLMETHOD_DEF(fun, numArgs) {#fun, (DL_FUNC) &fun, numArgs}

static const R_CallMethodDef callMethods[] = {
    /* cairoDevice.c */
    CALLMETHOD_DEF(do_asCairoDevice, 4),
    {NULL, NULL, 0}
};

void R_init_cairoDevice(DllInfo *info)
{
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
    R_useDynamicSymbols(info, TRUE);
}
