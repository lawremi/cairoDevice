#include <gtk/gtk.h>

/* extra backends */
#include <cairo/cairo-pdf.h>
#include <cairo/cairo-ps.h>
#include <cairo/cairo-svg.h>

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
  cairo_surface_t *surface; /* if non-NULL we have an alt surface like svg */
  gchar *filename;
	gint width, height;
  SEXP eventRho, eventResult;
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
static Rboolean Cairo_Open(NewDevDesc*, CairoDesc*, double, double, const gchar **);
static Rboolean Cairo_OpenEmbedded(NewDevDesc*, CairoDesc*, GtkWidget*);

#define SYMBOL_FONTFACE 5

#define BEGIN_SUSPEND_INTERRUPTS
#define END_SUSPEND_INTERRUPTS

/* R mouse events from non-public Graphics.h */
typedef enum {meMouseDown = 0,
	      meMouseUp,
	      meMouseMove} R_MouseEvent;
        
SEXP doMouseEvent(SEXP eventRho, NewDevDesc *dd, R_MouseEvent event,
			 int buttons, double x, double y);
       
/* and the key events */
typedef enum {knUNKNOWN = -1,
              knLEFT = 0, knUP, knRIGHT, knDOWN,
              knF1, knF2, knF3, knF4, knF5, knF6, knF7, knF8, knF9, knF10,
              knF11, knF12,
              knPGUP, knPGDN, knEND, knHOME, knINS, knDEL} R_KeyName;
           
SEXP doKeybd(SEXP eventRho, NewDevDesc *dd, R_KeyName rkey, char *keyname);

/* event handler */
void R_gtk_eventHandler(void *userData);
