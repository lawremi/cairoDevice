#include <gtk/gtk.h>

#include <locale.h>

#include <R.h>
#include <Rinternals.h>
#include <Rgraphics.h>
#include <Rdevices.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>

typedef struct {
	GtkWidget *window;			/* Graphics frame */
	GtkWidget *drawing;			/* widget to which we are drawing */
	GdkPixmap *pixmap;			/* off-screen drawable */
	cairo_t *cr;				/* the cairo context to which we draw */
	//PangoContext *pango;		/* cairo's pango context */
	gint width, height;
} CairoDesc;

/* Device driver actions */
static void Cairo_Activate(NewDevDesc *dd);
static void Cairo_Circle(double x, double y, double r,
		       R_GE_gcontext *gc,
		       NewDevDesc *dd);
static void Cairo_Clip(double x0, double x1, double y0, double y1, 
		     NewDevDesc *dd);
static void Cairo_Close(NewDevDesc *dd);
static void Cairo_Deactivate(NewDevDesc *dd);
static void Cairo_Hold(NewDevDesc *dd);
static Rboolean Cairo_Locator(double *x, double *y, NewDevDesc *dd);
static void Cairo_Line(double x1, double y1, double x2, double y2,
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static void Cairo_MetricInfo(int c,
			   R_GE_gcontext *gc,
			   double* ascent, double* descent,
			   double* width, NewDevDesc *dd);
static void Cairo_Mode(int mode, NewDevDesc *dd);
static void Cairo_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
static void Cairo_Polygon(int n, double *x, double *y, 
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void Cairo_Polyline(int n, double *x, double *y, 
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void Cairo_Rect(double x0, double y0, double x1, double y1,
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static void Cairo_Size(double *left, double *right,
		     double *bottom, double *top,
		     NewDevDesc *dd);
static double Cairo_StrWidth(char *str,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void Cairo_Text(double x, double y, char *str, 
		     double rot, double hadj, 
		     R_GE_gcontext *gc,
		     NewDevDesc *dd);
static Rboolean Cairo_Open(NewDevDesc*, CairoDesc*, double, double);
static Rboolean Cairo_OpenEmbedded(NewDevDesc*, CairoDesc*, GtkWidget*);

#define SYMBOL_FONTFACE 5

#define BEGIN_SUSPEND_INTERRUPTS
#define END_SUSPEND_INTERRUPTS
