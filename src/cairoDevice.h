#include <gtk/gtk.h>

#include <cairo.h>

/* extra backends */
#if CAIRO_VERSION > CAIRO_VERSION_ENCODE(1,2,0)
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <cairo-svg.h>
#endif

#include <locale.h>

#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsEngine.h>

typedef struct _Cairo_locator_info {
    guint x;
    guint y;
    gboolean button1;
    guint handler_id;
    gboolean active;
} CairoLocator;

#if R_GE_version < 7
typedef struct _CairoEvent {
  SEXP rho;
  SEXP result;
  gboolean active;
} CairoEvent;
#endif

typedef struct {
  GtkWidget *window;			/* Graphics frame */
  GtkWidget *drawing;			/* widget to which we are drawing */
  GdkPixmap *pixmap;			/* off-screen drawable */
  cairo_t *cr;				/* the cairo context
                                           to which we draw */
  cairo_t *cr_custom; /* custom cairo context from R */
  cairo_surface_t *surface; /* if non-NULL we have an alt surface like svg */
  gchar *filename; /* filename for certain Cairo backends */
	gint width, height; /* width and height of device */
#if R_GE_version < 7
  CairoEvent *event; /* stores information for 'getGraphicsEvent' support */
#endif
  CairoLocator *locator; /* stores information for 'locator' support */
  int holdlevel; /* apparently we have to track the hold level */
} CairoDesc;

/* Device driver actions */
static void Cairo_Activate(pDevDesc dd);
static void Cairo_Circle(double x, double y, double r,
		       const pGEcontext gc,
		       pDevDesc dd);
static void Cairo_Clip(double x0, double x1, double y0, double y1, 
		     pDevDesc dd);
static void Cairo_Close(pDevDesc dd);
static void Cairo_Deactivate(pDevDesc dd);
static Rboolean Cairo_Locator(double *x, double *y, pDevDesc dd);
static void Cairo_Line(double x1, double y1, double x2, double y2,
		     const pGEcontext gc,
		     pDevDesc dd);
static void Cairo_MetricInfo(int c,
			   const pGEcontext gc,
			   double* ascent, double* descent,
			   double* width, pDevDesc dd);
static void Cairo_Mode(int mode, pDevDesc dd);
static void Cairo_NewPage(const pGEcontext gc, pDevDesc dd);
static void Cairo_Polygon(int n, double *x, double *y, 
			const pGEcontext gc,
			pDevDesc dd);
static SEXP Cairo_Cap(pDevDesc dd);
static void Cairo_Raster(unsigned int *raster, int w, int h,
                         double x, double y, 
                         double width, double height,
                         double rot, 
                         Rboolean interpolate,
                         const pGEcontext gc, pDevDesc dd);

static void Cairo_Polyline(int n, double *x, double *y, 
			 const pGEcontext gc,
			 pDevDesc dd);
static void Cairo_Rect(double x0, double y0, double x1, double y1,
		     const pGEcontext gc,
		     pDevDesc dd);
static void Cairo_Size(double *left, double *right,
		     double *bottom, double *top,
		     pDevDesc dd);
static double Cairo_StrWidth(const char *str,
			   const pGEcontext gc,
			   pDevDesc dd);
static void Cairo_Text(double x, double y, const char *str, 
		     double rot, double hadj, 
		     const pGEcontext gc,
		     pDevDesc dd);
static Rboolean Cairo_Open(pDevDesc, CairoDesc*, double, double, const gchar **);
static Rboolean Cairo_OpenEmbedded(pDevDesc, CairoDesc*, GtkWidget*);

#define SYMBOL_FONTFACE 5

void	R_WriteConsole(const char*, int);

/* event handler */
void R_gtk_eventHandler(void *userData);
