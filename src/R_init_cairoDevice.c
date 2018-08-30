#include <R_ext/Rdynload.h>
#include <Rinternals.h>

void do_Cairo(double *in_width, double *in_height, double *in_pointsize,
	      char **surface);

SEXP do_asCairoDevice(SEXP widget, SEXP pointsize, SEXP width, SEXP height);

void loadGTK(int *success);

void cleanupGTK();

#define CALLMETHOD_DEF(fun, numArgs) {#fun, (DL_FUNC) &fun, numArgs}

static const R_CallMethodDef callMethods[] = {
    /* cairoDevice.c */
    CALLMETHOD_DEF(do_Cairo, 4),
    CALLMETHOD_DEF(do_asCairoDevice, 4),
    /* gtk.c */
    CALLMETHOD_DEF(loadGTK, 1),
    CALLMETHOD_DEF(cleanupGTK, 0),
    {NULL, NULL, 0}
};

void R_init_cairoDevice(DllInfo *info)
{
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
    R_useDynamicSymbols(info, TRUE);
}
