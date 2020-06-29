#include "cairoDevice.h"

// All measurements within this file are made in points (unless otherwise stated)
#define POINTS_PER_INCH 72
#define CURSOR		GDK_CROSSHAIR		/* Default cursor */
#define MM_PER_INCH	25.4			/* mm -> inch conversion */

static PangoStyle  slant[]  = { PANGO_STYLE_NORMAL, PANGO_STYLE_OBLIQUE };
static PangoWeight weight[] = { PANGO_WEIGHT_NORMAL, PANGO_WEIGHT_BOLD };

// Resolution is measured in dpi (dots per inch)
double gResolutionX = 72;
double gResolutionY = 72;

// Sets resolution independently for X and Y directions 
void
setResolutionXY(double resX, double resY) {
  //printf("Resolution was (%f,%f)\n", gResolutionX, gResolutionY);
  gResolutionX = resX;
  gResolutionY = resY;
  //printf("Resolution is now (%f,%f)\n", gResolutionX, gResolutionY);
}

// Sets resolution assuming X and Y are identical
void
setResolution(double res) {
  setResolutionXY(res,res);
}

// A function exposed to R allowing it to change the resolution
SEXP 
Cairo_Set_Resolution(SEXP res) {
  double resolution = asReal(res);
  setResolution(resolution);
  return ScalarReal(resolution);
}

// Obtains X and Y resolutions of screen surface and updates global variables
void
setResolutionFromScreen() {
  double width, widthMM;
  double height, heightMM;  
  double resX, resY;

  // X direction
  width = gdk_screen_width();
  widthMM = gdk_screen_width_mm();
  resX = width/widthMM * MM_PER_INCH;

  // Y direction
  height = gdk_screen_height();
  heightMM = gdk_screen_height_mm();
  resY = height/heightMM * MM_PER_INCH;

  // Update global variables
  setResolutionXY(resX, resY);
}



CairoDesc* createCairoDesc() {
  return(g_new0(CairoDesc, 1));
}
void freeCairoDesc(pDevDesc dd) {
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  
  if (!cd)
    return;
  dd->deviceSpecific = NULL;
  
  if(cd->pixmap)
    g_object_unref(cd->pixmap);

  if(cd->drawing)
    gtk_widget_destroy(cd->drawing);
  
  if(cd->window)
    gtk_widget_destroy(cd->window);

  if(cd->cr) {
    if (!cd->cr_custom)
      cairo_show_page(cd->cr);
    else cairo_restore(cd->cr);
    cairo_destroy(cd->cr);
  }
  
  if (cd->filename)
    g_free(cd->filename);
	
  if (cd->surface)
    cairo_surface_destroy(cd->surface);
  
  g_free(cd);
}

static void activateDevice(pDevDesc dev)
{
  pGEDevDesc gdd;
  CairoDesc *cd = (CairoDesc *)dev->deviceSpecific;
  GObject *obj = cd->drawing ? G_OBJECT(cd->drawing) : G_OBJECT(cd->pixmap);

  gdd = GEcreateDevDesc(dev);
  GEaddDevice2(gdd, "Cairo");
  if (obj) {
    SEXP devnum = ScalarInteger(ndevNumber(dev) + 1);
    R_PreserveObject(devnum);
    g_object_set_data_full(obj, ".devnum", devnum, (GDestroyNotify)R_ReleaseObject);
  }
}


static PangoContext *getPangoContext(CairoDesc *cd)
{
  if (cd->drawing)
    return(gtk_widget_get_pango_context(cd->drawing));
  return(gdk_pango_context_get());
}

static PangoFont *loadFont(PangoFontDescription *fontdesc, CairoDesc *cd)
{
  return(pango_context_load_font(getPangoContext(cd), fontdesc));
}

static PangoFontDescription *getBaseFont(CairoDesc *cd)
{
  return(pango_font_description_from_string("Verdana"));
}

static gboolean initDevice(pDevDesc dd)
{
  CairoDesc *cd;
  GdkCursor *cursor;
  gdouble left, right, bottom, top;
	
  cd = (CairoDesc *) dd->deviceSpecific;
	
  Cairo_Size(&left, &right, &bottom, &top, dd);
  //printf("Cairo_Size returned (%f %f %f %f)\n", left, right, bottom, top);
  /*g_debug("%f %f %f %f" , left, right, bottom, top);*/
  dd->left = left;
  cd->width = dd->right = right;
  cd->height = dd->bottom = bottom;
  dd->top = top;
  
  /* set up crosshair cursor */
  if (cd->drawing) {
    cursor = gdk_cursor_new(CURSOR);
    gdk_window_set_cursor(cd->drawing->window, cursor);
    gdk_cursor_unref(cursor);
  }
  
  if(cd->cr) {
    if (!cd->cr_custom) {
      cairo_show_page(cd->cr);
      cairo_destroy(cd->cr);
    } else cairo_restore(cd->cr);
  }
  if(cd->pixmap && cd->drawing)
    g_object_unref(cd->pixmap);
  
  /* create pixmap and cairo context for drawing, save default state */
  if(right > 0 && bottom > 0) {
    if (cd->drawing) {
      cd->pixmap = gdk_pixmap_new(cd->drawing->window, 
				  right*gResolutionX/POINTS_PER_INCH, 
				  bottom*gResolutionY/POINTS_PER_INCH, -1);
    }
    if (cd->surface) {
      cd->cr = cairo_create(cd->surface);
    }
    else if (cd->cr_custom) {
      cd->cr = cd->cr_custom;
    }
    else cd->cr = gdk_cairo_create(cd->pixmap);
  //cairo_set_antialias(cd->cr, CAIRO_ANTIALIAS_NONE);
    //cd->pango = pango_cairo_font_map_create_context(
    //				PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
    //pango_cairo_context_set_resolution(cd->pango, 84);
    /* sync the size with the device */
  }

  // Change the CTM (current transformation matrix) to scale for different resolutions
  cairo_scale(cd->cr, gResolutionX/POINTS_PER_INCH,gResolutionY/POINTS_PER_INCH);
  //printf("  scaling by %f, %f\n", gResolutionX/POINTS_PER_INCH,gResolutionY/POINTS_PER_INCH);
    cairo_save(cd->cr);
  
  return FALSE;
}

static void resize(pDevDesc dd)
{
    GEplayDisplayList(desc2GEDesc(dd));
}

static gboolean realize_event(GtkWidget *widget, pDevDesc dd)
{
  g_return_val_if_fail(dd != NULL, FALSE);
  initDevice(dd);
  return(FALSE);
}
static gboolean realize_embedded(GtkWidget *widget, pDevDesc dd)
{ /* different from above in that embedded device needs to be activated,
     since it wasn't when the device was created */
  g_return_val_if_fail(dd != NULL, FALSE);
  initDevice(dd);
  activateDevice(dd);
  return(FALSE);
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event, pDevDesc dd)
{
  CairoDesc *cd;
    
  g_return_val_if_fail(dd != NULL, FALSE);
  cd = (CairoDesc *)dd->deviceSpecific;
  g_return_val_if_fail(cd != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(cd->drawing), FALSE);

  gint current_width = ((double)cd->drawing->allocation.width/gResolutionX*POINTS_PER_INCH);
  gint current_height = ((double)cd->drawing->allocation.height/gResolutionY*POINTS_PER_INCH);

  if(cd->width != current_width
     || cd->height != current_height) {
    resize(dd); 
  }
  else if(cd->pixmap) {
    gdk_draw_drawable(cd->drawing->window, cd->drawing->style->bg_gc[GTK_STATE_NORMAL], 
                      cd->pixmap,
                      event->area.x, event->area.y,
                      event->area.x, event->area.y,
                      event->area.width, event->area.height);
  } //else {
  //GEplayDisplayList((GEDevDesc*) Rf_GetDevice(Rf_devNumber((DevDesc*)dd)));
  //}
    
  return FALSE;
}

#if R_GE_version < 7
static void event_finish(pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  dd->onExit = NULL;
  dd->gettingEvent = FALSE;
  cd->event->active = FALSE;
  cd->event = NULL;
}
static void event_maybe_finish(pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (cd->event->result && cd->event->result != R_NilValue)
    event_finish(dd);
}
static void CairoEvent_onExit(pDevDesc dd)
{
  event_finish(dd);
}
#endif

/* converts the Gdk buttons to the button masks used by R's gevent.c */
#define R_BUTTON(button) pow(2, (button) - 1)
#define R_BUTTONS(state) (state) & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK) >> 8

static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event, pDevDesc dd)
{
  if (dd->gettingEvent) {
#if R_GE_version < 7
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseDown,
                                     R_BUTTON(event->button), event->x, event->y);
    event_maybe_finish(dd);
#else
    doMouseEvent(dd, meMouseDown, R_BUTTON(event->button), event->x, event->y);
#endif

  }
  return(FALSE);
}

static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event, pDevDesc dd)
{
  if (dd->gettingEvent) {
#if R_GE_version < 7
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseUp,
                                     R_BUTTON(event->button), event->x, event->y);
    event_maybe_finish(dd);
#else
    doMouseEvent(dd, meMouseUp, R_BUTTON(event->button), event->x, event->y);
#endif
  }
  return(FALSE);
}

static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, pDevDesc dd)
{
  if (dd->gettingEvent) {
#if R_GE_version < 7
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseMove,
                                     R_BUTTONS(event->state), event->x, event->y);
    event_maybe_finish(dd);
#else
    doMouseEvent(dd, meMouseMove, R_BUTTONS(event->state), event->x, event->y);
#endif
  }
  return(FALSE);
}

static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event, pDevDesc dd)
{
  if (dd->gettingEvent) {
#if R_GE_version < 7
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
    cd->event->result = doKeybd(cd->event->rho, dd, -1, gdk_keyval_name(event->keyval));
    event_maybe_finish(dd);
#else
    doKeybd(dd, -1, gdk_keyval_name(event->keyval));
#endif
  }
  return(FALSE);
}

#if R_GE_version < 7
static SEXP Cairo_GetEvent(SEXP rho, const char *prompt)
{
  pGEDevDesc dd = GEcurrentDevice();
  CairoDesc *cd = (CairoDesc *) dd->dev->deviceSpecific;
  CairoEvent *event = g_new0(CairoEvent, 1);
  SEXP result = R_NilValue;
    
  if (cd->event) 
    error("recursive use of getGraphicsEvent not supported");
    
  cd->event = event;
  event->rho = rho;
  event->result = R_NilValue;
    
  dd->dev->gettingEvent = TRUE;
    
  R_WriteConsole(prompt, strlen(prompt));
  R_WriteConsole("\n", 1);
  R_FlushConsole();

  dd->dev->onExit = CairoEvent_onExit;  /* install callback for cleanup */
  event->active = TRUE;
  while (event->active) {
    R_gtk_eventHandler(NULL);
  }
    
  if (event->result)
    result = event->result;
  g_free(event);
  return result;
}
#else
static void Cairo_EventHelper(pDevDesc dd, int code)
{
  if (code == 1) {
    if (isEnvironment(dd->eventEnv)) {
      SEXP prompt = findVar(install("prompt"), dd->eventEnv);
      if (length(prompt) == 1) {
	const char* cprompt = CHAR(STRING_ELT(prompt, 0));
        R_WriteConsole(cprompt, strlen(cprompt));
        R_WriteConsole("\n", 1);
        R_FlushConsole();
      }
    }
  } else if (code == 2) 
    R_gtk_eventHandler(NULL);
  
  return;
}
#endif

static void kill_cairo(pDevDesc dd)
{
    GEkillDevice(desc2GEDesc(dd));
}

static void unrealize_cb(GtkWidget *widget, pDevDesc dd) {
  g_return_if_fail(dd != NULL);
  if (dd->deviceSpecific) { /* make sure not called from freeCairoDesc */
    /* don't try to destroy the widget, since it's already being destroyed */
    ((CairoDesc *) dd->deviceSpecific)->drawing = NULL;
    kill_cairo(dd);
  }
}
static gint delete_event(GtkWidget *widget, GdkEvent *event, pDevDesc dd)
{
  g_return_val_if_fail(dd != NULL, FALSE);
  kill_cairo(dd);
  return TRUE;
}

static void setupWidget(GtkWidget *drawing, pDevDesc dd)
{
  gtk_widget_add_events(drawing, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(drawing), "expose_event", G_CALLBACK(expose_event), dd);
  g_signal_connect(G_OBJECT(drawing), "motion_notify_event", 
                   G_CALLBACK(motion_notify_event), dd);
  g_signal_connect(G_OBJECT(drawing), "button_release_event", 
                   G_CALLBACK(button_release_event), dd);
  g_signal_connect(G_OBJECT(drawing), "button_press_event", 
                   G_CALLBACK(button_press_event), dd);
}

static Rboolean Cairo_OpenEmbedded(pDevDesc dd, CairoDesc *cd, GtkWidget *drawing)
{
  dd->deviceSpecific = cd;
  cd->drawing = drawing;
  // initialize
  /*g_return_val_if_fail(GTK_WIDGET_REALIZED(drawing), FALSE);*/
  if (GTK_WIDGET_REALIZED(drawing))
    initDevice(dd);
  else g_signal_connect(G_OBJECT(drawing), "realize", G_CALLBACK(realize_embedded), dd);
  // hook it up for drawing and user events
  setupWidget(drawing, dd); // free the device when it's no longer able to draw
  g_signal_connect(G_OBJECT(drawing), "unrealize", G_CALLBACK(unrealize_cb), dd);
  return(TRUE);
}
static Rboolean Cairo_OpenOffscreen(pDevDesc dd, CairoDesc *cd, GdkDrawable *drawing)
{
  dd->deviceSpecific = cd;
  cd->pixmap = drawing;
  g_object_ref(G_OBJECT(cd->pixmap));
  // initialize
  initDevice(dd);		 
  return(TRUE);
}

static Rboolean Cairo_OpenCustom(pDevDesc dd, CairoDesc *cd, double w, double h,
                                 cairo_t *cr)
{
  dd->deviceSpecific = cd;
  cd->cr_custom = cairo_reference(cr);
  cd->width = w;
  cd->height = h;
  // initialize
  initDevice(dd);		 
  return(TRUE);
}

static Rboolean Cairo_Open(pDevDesc dd, CairoDesc *cd,	double w, double h, 
                           const gchar **surface_info)
{	
  dd->deviceSpecific = cd;
	
  if (strcmp(surface_info[0], "screen")) {
    cairo_surface_t *surface;
    double width, height;
    if (!strcmp(surface_info[0], "png")) {
      width = w * gResolutionX/POINTS_PER_INCH;
      height = h * gResolutionY/POINTS_PER_INCH;
      surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
      cd->filename = g_strdup(surface_info[1]);
    } else {
      
      setResolution(POINTS_PER_INCH);
      width = w;
      height = h;
#ifdef CAIRO_HAS_PDF_SURFACE
      if (!strcmp(surface_info[0], "pdf"))
        surface = cairo_pdf_surface_create(surface_info[1], width, height);
      else
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
        if (!strcmp(surface_info[0], "svg"))
          surface = cairo_svg_surface_create(surface_info[1], width, height);
        else
#endif
#ifdef CAIRO_HAS_PS_SURFACE
          if (!strcmp(surface_info[0], "ps"))
            surface = cairo_ps_surface_create(surface_info[1], width, height);
          else
#endif
            {
              warning("Unknown surface type: %s", surface_info[0]);
              return(FALSE);
            }
    }
    cd->width = w;
    cd->height = h;
    cd->surface = surface;
    return(TRUE);
  }

  // Set resolution
  setResolutionFromScreen(); 
  
  cd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(cd->window), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(cd->window), 
                              w * gResolutionX/POINTS_PER_INCH, 
			      h * gResolutionY/POINTS_PER_INCH);

  /* create drawingarea */
  cd->drawing = gtk_drawing_area_new();
  
  /* connect to signal handlers, etc */
  g_signal_connect(G_OBJECT (cd->drawing), "realize",
                   G_CALLBACK (realize_event), dd);
	
  /* place and realize the drawing area */
  gtk_container_add(GTK_CONTAINER(cd->window), cd->drawing);

  /* connect to signal handlers, etc */
  setupWidget(cd->drawing, dd);
  g_signal_connect(G_OBJECT(cd->window), "delete_event",
                   G_CALLBACK(delete_event), dd);
  g_signal_connect(G_OBJECT(cd->window), "key_press_event", 
                   G_CALLBACK(key_press_event), dd);
  
  gtk_widget_show_all(cd->window);

  return(TRUE);
}

static PangoFontDescription *getFont(CairoDesc *cd, const pGEcontext gc)
{
  PangoFontDescription *fontdesc;
  gint face = gc->fontface;
  double size = gc->cex * gc->ps;

  if (face < 1 || face > 5) face = 1;

  fontdesc = pango_font_description_new();
  if (face == 5)
    pango_font_description_set_family(fontdesc, "symbol");
  else {
    char *fm = gc->fontfamily;
    /*if(!strcmp(fm, "mono")) fm = "courier";
    else if(!strcmp(fm, "serif")) fm = "times";
    else if(!strcmp(fm, "sans")) fm = "helvetica";*/
    pango_font_description_set_family(fontdesc, fm[0] ? fm : "Verdana");
    if(face == 2 || face == 4)
      pango_font_description_set_weight(fontdesc, PANGO_WEIGHT_BOLD);
    if(face == 3 || face == 4)
      pango_font_description_set_style(fontdesc, PANGO_STYLE_OBLIQUE);
  }
  pango_font_description_set_size(fontdesc, PANGO_SCALE * size);

  return fontdesc;
}

static PangoLayout *layoutText(PangoFontDescription *desc, const char *str, 
			       CairoDesc *cd)
{
  PangoLayout *layout;
	
  //pango_cairo_update_context(cd->cr, cd->pango);
  //layout = pango_layout_new(cd->pango);
  /*if (cd->drawing)
    layout = gtk_widget_create_pango_layout(cd->drawing, NULL);
    else layout = pango_layout_new(gdk_pango_context_get());*/
  layout = pango_cairo_create_layout(cd->cr);
  pango_layout_set_font_description(layout, desc);
  pango_layout_set_text(layout, str, -1);
  return(layout);
}

static void
text_extents(PangoFontDescription *desc, CairoDesc *cd, const pGEcontext gc, 
             const gchar *text,
	     gint *lbearing, gint *rbearing, 
	     gint *width, gint *ascent, gint *descent, gboolean ink)
{
  PangoLayout *layout;
  PangoRectangle rect, irect;
	
  layout = layoutText(desc, text, cd);
  
  pango_layout_line_get_pixel_extents(pango_layout_get_line(layout, 0), &irect,
                                      &rect);

  if (!ink) {
    if(ascent)
      *ascent = PANGO_ASCENT(rect);
    if(descent)
      *descent = PANGO_DESCENT(rect);
    if(width)
      *width = rect.width;
    if(lbearing)
      *lbearing = PANGO_LBEARING(rect);
    if(rbearing)
      *rbearing = PANGO_RBEARING(rect);
  } else {
    if(ascent)
      *ascent = PANGO_ASCENT(irect);
    if(descent)
      *descent = PANGO_DESCENT(irect);
    if(width)
      *width = irect.width;
    if(lbearing)
      *lbearing = PANGO_LBEARING(irect);
    if(rbearing)
      *rbearing = PANGO_RBEARING(irect);
  }
  
  g_object_unref(layout);
}

static void font_metrics(PangoFontDescription *desc, CairoDesc *cd, 
                         gint *width, gint *ascent, gint *descent)
{
  PangoFontMetrics *metrics;
  
  metrics = pango_context_get_metrics(getPangoContext(cd),	desc, NULL);
	
  *ascent = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics));
  *descent = PANGO_PIXELS(pango_font_metrics_get_descent(metrics));
  *width = PANGO_PIXELS(pango_font_metrics_get_approximate_char_width(metrics));
	
  pango_font_metrics_unref(metrics);
}

static void setColor(cairo_t *cr, int color)
{
  cairo_set_source_rgba(cr, R_RED(color) / 255.0, R_GREEN(color) / 255.0, 
                        R_BLUE(color) / 255.0, R_ALPHA(color) / 255.0);
}

/* set the line type */
static void setLineType(cairo_t *cr, const pGEcontext gc)
{
  cairo_line_cap_t cap = CAIRO_LINE_CAP_ROUND;
  cairo_line_join_t join = CAIRO_LINE_JOIN_ROUND;
  static double dashes[8];
  gint i;
  
  cairo_set_line_width(cr, gc->lwd);
	
  switch(gc->lend) {
  case GE_ROUND_CAP:
    cap = CAIRO_LINE_CAP_ROUND;
    break;
  case GE_BUTT_CAP:
    cap = CAIRO_LINE_CAP_BUTT;
    break;
  case GE_SQUARE_CAP:
    cap = CAIRO_LINE_CAP_SQUARE;
    break;
  }
  cairo_set_line_cap(cr, cap);
	
  switch(gc->ljoin) {
  case GE_ROUND_JOIN:
    join = CAIRO_LINE_JOIN_ROUND;
    break;
  case GE_MITRE_JOIN:
    join = CAIRO_LINE_JOIN_MITER;
    cairo_set_miter_limit(cr, gc->lmitre);
    break;
  case GE_BEVEL_JOIN:
    join = CAIRO_LINE_JOIN_BEVEL;
    break;
  }
  cairo_set_line_join(cr, join);

  for(i = 0; i < 8 && gc->lty & 15; i++) {
    dashes[i] = gc->lty & 15;
    gc->lty = gc->lty >> 4;
  }

  /* set dashes */
  cairo_set_dash(cr, dashes, i, 0);
}

static void drawShape(cairo_t *cr, const pGEcontext gc)
{
  gboolean stroke = R_ALPHA(gc->col) > 0 && gc->lty != -1;
  gboolean fill = R_ALPHA(gc->fill) > 0;
  if (fill) {
    cairo_antialias_t antialias = cairo_get_antialias(cr);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    setColor(cr, gc->fill);
    if (stroke) {
      cairo_fill_preserve(cr);
    } else {
      cairo_fill(cr);
    }
    cairo_set_antialias(cr, antialias);
  }
  if (stroke) {
    setColor(cr, gc->col);
    setLineType(cr, gc);
    cairo_stroke(cr);
  }
}

static void drawRect(cairo_t *cr, double x0, double y0, double x1, double y1, 
                     const pGEcontext gc) 
{
  cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
  drawShape(cr, gc);
}

static double Cairo_StrWidth(const char *str, const pGEcontext gc, pDevDesc dd)
{
  gint width;
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  PangoFontDescription *desc = getFont(cd, gc);
  text_extents(desc, cd, gc, str, NULL, NULL, &width, NULL, NULL, FALSE);
  pango_font_description_free(desc);
	
  return (double) width;
}

static void Cairo_MetricInfo(int c, const pGEcontext gc,
                             double* ascent, double* descent, double* width, pDevDesc dd)
{
  gchar text[16];
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  gint iascent, idescent, iwidth;
  int Unicode = mbcslocale;
	
  PangoFontDescription *desc = getFont(cd, gc);
	
  if (!c) 
    font_metrics(desc, cd, &iwidth, &iascent, &idescent);
  else {
    /*if (!c) c = 77;*/
    if(c < 0) {c = -c; Unicode = 1;} 
  
  if(Unicode || c >= 128)
    Rf_ucstoutf8(text, c);
  else
    g_snprintf(text, 2, "%c", (gchar) c);
  text_extents(desc, cd, gc, text, NULL, NULL, &iwidth, &iascent, &idescent,
               TRUE);
      }
	
  *ascent = iascent;
  *descent = idescent;
  *width = iwidth;
	
  //Rprintf("%f %f %f\n", *ascent, *descent, *width);
	
  pango_font_description_free(desc);
}

static void Cairo_Clip(double x0, double x1, double y0, double y1, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  gint cx, cy, cw, ch;
	
  cx = (int) MIN(x0, x1);
  cw = abs( (int) x0 - (int) x1) + 1;

  cy = (int) MIN(y0, y1);
  ch = abs( (int) y0 - (int) y1) + 1;

  cairo_restore(cd->cr); // get rid of last clip
  cairo_save(cd->cr);
  cairo_rectangle(cd->cr, cx, cy, cw, ch);
  cairo_clip(cd->cr);
	
  //Rprintf("clip: %f %f %f %f\n", cx, cy, cw, ch);
}

static void Cairo_Size(double *left, double *right, double *bottom, double *top,
                       pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  gint width = cd->width, height = cd->height;

  /* we have to use the allocation width/height if we have a widget, because
     the new backing pixmap is not created until later */
  if (cd->drawing) {
    if (GTK_WIDGET_MAPPED(cd->drawing)) {
      width = (double)cd->drawing->allocation.width / gResolutionX * POINTS_PER_INCH ;
      height = (double)cd->drawing->allocation.height / gResolutionY * POINTS_PER_INCH;
    } else {
      width = height = 1000; // hack to get R to draw before first exposure
    }
  } else if (GDK_IS_DRAWABLE(cd->pixmap)) {
    gdk_drawable_get_size(cd->pixmap, &width, &height);
    width = (double)width / gResolutionX * POINTS_PER_INCH;
    height = (double)height / gResolutionY * POINTS_PER_INCH;
  }

  *left = 0.0;
  *right = (gdouble)width;
  *top = 0.0;
  *bottom = (gdouble)height;
}

/* clear the drawing area */
static void Cairo_NewPage(const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = dd->deviceSpecific;
  cairo_t *cr;
  
  initDevice(dd);

  cr = cd->cr;

  if (!R_OPAQUE(gc->fill))
    cairo_set_source_rgb(cr, 1, 1, 1);
  else setColor(cr, gc->fill);

  cairo_new_path(cr);
  cairo_paint(cr);

  if (cd->drawing)
    gtk_widget_queue_draw(cd->drawing);
}

/** kill off the window etc
 */
static void Cairo_Close(pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (dd->onExit)
    dd->onExit(dd);
  if (cd->filename) /* we have an image surface, write to png */
    cairo_surface_write_to_png(cd->surface, cd->filename);
  freeCairoDesc(dd);
}

#define title_text_inactive "R graphics device %d"
#define title_text_active "R graphics device %d - Active"

void
setActive(pDevDesc dd, gboolean active)
{
  CairoDesc *cd;
  gint devnum;
  gchar *title_text;

  cd = (CairoDesc *) dd->deviceSpecific;
  g_return_if_fail(cd != NULL);
    
  if (!cd->window)
    return;

  devnum = ndevNumber(dd) + 1;

  if (active)
    title_text = g_strdup_printf(title_text_active, devnum);
  else title_text = g_strdup_printf(title_text_inactive, devnum);

  gtk_window_set_title(GTK_WINDOW(cd->window), title_text);

  g_free(title_text);
}

static void Cairo_Activate(pDevDesc dd)
{
  setActive(dd, TRUE);
}

static void Cairo_Deactivate(pDevDesc dd)
{
  setActive(dd, FALSE);
}	

static void Cairo_Rect(double x0, double y0, double x1, double y1,
                       const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawRect(cd->cr, x0, y0, x1, y1, gc);
  cairo_restore(cd->cr);
}

static void drawPath(cairo_t *cr,
                     double *x, double *y, 
                     int npoly, int *nper,
                     Rboolean winding,
                     const pGEcontext gc)
{
  int i, j, z;

  cairo_new_path(cr);
  
  for (i = 0, z = 0; i < npoly; i++) {
    cairo_move_to(cr, x[z], y[z]);
    z++;
    for (j = 1; j < nper[i]; j++, z++) {
      cairo_line_to(cr, x[z], y[z]);
    }
    cairo_close_path(cr);
  }

  cairo_set_fill_rule(cr, winding ? CAIRO_FILL_RULE_WINDING :
                      CAIRO_FILL_RULE_EVEN_ODD);
  
  drawShape(cr, gc);
}

static void Cairo_Path(double *x, double *y, 
                       int npoly, int *nper,
                       Rboolean winding,
                       const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawPath(cd->cr, x, y, npoly, nper, winding, gc);
  cairo_restore(cd->cr);
}

static void drawCircle(cairo_t *cr, double x, double y, double r, const pGEcontext gc)
{
  cairo_move_to(cr, x+r, y);
  cairo_translate(cr, x, y);
  cairo_arc(cr, 0, 0, r, 0, 2 * M_PI);
  //cairo_arc(cr, x, y, r, 0, 2 * M_PI);	
  drawShape(cr, gc);
}

static void Cairo_Circle(double x, double y, double r,
                         const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawCircle(cd->cr, x, y, r, gc);
  cairo_restore(cd->cr);
}

static void drawLine(cairo_t *cr, double x1, double y1, double x2, double y2,
                     const pGEcontext gc)
{
  cairo_move_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
	
  setColor(cr, gc->col);
  setLineType(cr, gc);
  cairo_stroke(cr);
}

static void Cairo_Line(double x1, double y1, double x2, double y2,
                       const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  //g_debug("line");
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawLine(cd->cr, x1, y1, x2, y2, gc);
  cairo_restore(cd->cr);
}

static void polypath(cairo_t *cr, int n, double *x, double *y)
{
  gint i;
  g_return_if_fail(n > 0);
  cairo_move_to(cr, x[0], y[0]);
  for (i = 1; i < n; i++)
    cairo_line_to(cr, x[i], y[i]);
}

static void drawPolyline(cairo_t *cr, int n, double *x, double *y, const pGEcontext gc)
{
  polypath(cr, n, x, y);
  setColor(cr, gc->col);
  setLineType(cr, gc);
  cairo_stroke(cr);
}

static void Cairo_Polyline(int n, double *x, double *y, 
                           const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  //g_debug("polyline");
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawPolyline(cd->cr, n, x, y, gc);
  cairo_restore(cd->cr);
}

static void drawPolygon(cairo_t *cr, int n, double *x, double *y, const pGEcontext gc)
{
  polypath(cr, n, x, y);
  cairo_close_path(cr);
  drawShape(cr, gc);
}

static void Cairo_Polygon(int n, double *x, double *y, 
                          const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawPolygon(cd->cr, n, x, y, gc);
  cairo_restore(cd->cr);
}

#if R_GE_version > 6
/* Adapted from R's modules/X11/cairoX11.c */
static void Cairo_Raster(unsigned int *raster, int w, int h,
                         double x, double y, 
                         double width, double height,
                         double rot, 
                         Rboolean interpolate,
                         const pGEcontext gc, pDevDesc dd)
{
  char *vmax = vmaxget();
  int i;
  cairo_surface_t *image;
  unsigned char *imageData;
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  
  imageData = (unsigned char *) R_alloc(4*w*h, sizeof(unsigned char));
  /* The R ABGR needs to be converted to a Cairo ARGB 
   * AND values need to by premultiplied by alpha 
   */
  for (i=0; i<w*h; i++) {
    int alpha = R_ALPHA(raster[i]);
    imageData[i*4 + 3] = alpha;
    if (alpha < 255) {
      imageData[i*4 + 2] = R_RED(raster[i]) * alpha / 255;
      imageData[i*4 + 1] = R_GREEN(raster[i]) * alpha / 255;
      imageData[i*4 + 0] = R_BLUE(raster[i]) * alpha / 255;
    } else {
      imageData[i*4 + 2] = R_RED(raster[i]);
      imageData[i*4 + 1] = R_GREEN(raster[i]);
      imageData[i*4 + 0] = R_BLUE(raster[i]);
    }
  }
  image = cairo_image_surface_create_for_data(imageData, 
                                              CAIRO_FORMAT_ARGB32,
                                              w, h, 
                                              4*w);

  cairo_save(cd->cr);

  cairo_translate(cd->cr, x, y);
  cairo_rotate(cd->cr, -rot*M_PI/180);
  cairo_scale(cd->cr, width/w, height/h);
  /* Flip vertical first */
  cairo_translate(cd->cr, 0, h/2.0);
  cairo_scale(cd->cr, 1, -1);
  cairo_translate(cd->cr, 0, -h/2.0);

  cairo_set_source_surface(cd->cr, image, 0, 0);

  /* Use nearest-neighbour filter so that a scaled up image
   * is "blocky";  alternative is some sort of linear
   * interpolation, which gives nasty edge-effects
   */
  if (!interpolate) {
    cairo_pattern_set_filter(cairo_get_source(cd->cr), 
                             CAIRO_FILTER_NEAREST);
  }

  cairo_paint(cd->cr); 

  cairo_restore(cd->cr);
  cairo_surface_destroy(image);

  vmaxset(vmax);
}

static SEXP Cairo_Cap(pDevDesc dd)
{
  int i, j, k, nbytes, width, height, size, stride;
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  unsigned char *screenData;
  SEXP dim, raster = R_NilValue;
  unsigned int *rint;
  GdkPixbuf *pixbuf = NULL;
  
  if (!cd->pixmap)
    return raster;

  pixbuf =
    gdk_pixbuf_get_from_drawable(NULL, cd->pixmap, NULL, 0, 0, 0, 0, -1, -1);

  stride = gdk_pixbuf_get_rowstride(pixbuf);
  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);
  screenData = gdk_pixbuf_get_pixels(pixbuf);
  
  /* Should not happen */
  if (gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB ||
      gdk_pixbuf_get_bits_per_sample(pixbuf) != 8 ||
      gdk_pixbuf_get_has_alpha(pixbuf)) 
    return raster;
  
  size = width*height;
  nbytes = stride*height;

  PROTECT(raster = allocVector(INTSXP, size));

  /* Copy each byte of screen to an R matrix. 
   * The Cairo RGB24 needs to be converted to an R ABGR32 */
  rint = (unsigned int *) INTEGER(raster);
  for (i = 0, k = 0; i < nbytes; i += stride)
    for (j = i; j < i + width*3; j += 3)
      rint[k++] = 255<<24 | ((screenData[j])<<16 | 
                             (screenData[j + 1])<<8 |
                             (screenData[j + 2]));
  
  PROTECT(dim = allocVector(INTSXP, 2));
  INTEGER(dim)[0] = height;
  INTEGER(dim)[1] = width;
  setAttrib(raster, R_DimSymbol, dim);

  UNPROTECT(2);
  return raster;
}
#endif

static void drawText(double x, double y, const char *str, 
		     double rot, double hadj, CairoDesc *cd, const pGEcontext gc)
{
  PangoLayout *layout;
  gint ascent, lbearing, width;
  cairo_t *cr = cd->cr;

  PangoFontDescription *desc = getFont(cd, gc);

  layout = layoutText(desc, str, cd);
  
  text_extents(desc, cd, gc, str, &lbearing, NULL, &width, &ascent, NULL, FALSE);

  cairo_move_to(cr, x, y);
  cairo_rotate(cr, -1*rot);
  cairo_rel_move_to(cr, -lbearing - width*hadj, -ascent);
  setColor(cr, gc->col);

  pango_cairo_show_layout(cr, layout);
  
  g_object_unref(layout);
  pango_font_description_free(desc);
}

static void Cairo_Text(double x, double y, const char *str, 
                       double rot, double hadj, const pGEcontext gc, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  double rrot = DEG2RAD * rot;
	
  //Rprintf("%s @ %f %f\n", str, x, y);
	
  cairo_save(cd->cr);
  drawText(x, y, str, rrot, hadj, cd, gc);
  cairo_restore(cd->cr);
}

static void locator_finish(pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  /* clean up handler */
  g_signal_handler_disconnect(G_OBJECT(cd->drawing), cd->locator->handler_id);
  /* turn off locator */
  dd->onExit = NULL;
  cd->locator->active = FALSE;
}

static void locator_button_press(GtkWidget *widget,
				 GdkEventButton *event,
				 pDevDesc dd)
{
  CairoLocator *info = ((CairoDesc *) dd->deviceSpecific)->locator;

  info->x = event->x;
  info->y = event->y;
  if(event->button == 1)
    info->button1 = TRUE;
  else
    info->button1 = FALSE;
    
  locator_finish(dd);
}

static void CairoLocator_onExit(pDevDesc dd)
{
  locator_finish(dd);
}

static Rboolean Cairo_Locator(double *x, double *y, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  CairoLocator *info;
  gboolean button1;
    
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(cd->drawing), FALSE);
  
  if (cd->holdlevel > 0)
    error("attempt to use the locator after dev.hold()");
  
  info = g_new0(CairoLocator, 1);
  cd->locator = info;
    
  /* force update to show identity */
  gtk_widget_queue_draw(cd->drawing);
  gdk_window_process_updates(cd->drawing->window,TRUE);
  gdk_flush();

  /* Flush any pending events */
  while(gtk_events_pending())
    gtk_main_iteration();

  /* connect signal */
  info->handler_id = g_signal_connect(G_OBJECT(cd->drawing),
                                      "button-press-event",
                                      G_CALLBACK(locator_button_press), dd);
    
  /* and an exit handler in case the window gets closed */
  dd->onExit = CairoLocator_onExit;
    
  /* run the handler */
  info->active = TRUE;
  while (info->active) {
    R_gtk_eventHandler(NULL);
  }
    
  *x = (double) info->x / gResolutionX * POINTS_PER_INCH;
  *y = (double) info->y / gResolutionY * POINTS_PER_INCH;
  button1 = info->button1;

  g_free(info);
    
  if(button1)
    return TRUE;
  return FALSE;
}

static void busy(CairoDesc *cd) {
  if (cd->drawing) {
    GdkCursor *cursor = gdk_cursor_new(GDK_WATCH);
    gdk_window_set_cursor(cd->drawing->window, cursor);
    gdk_cursor_unref(cursor);
  }
}

static void idle(CairoDesc *cd) {
  if (cd->drawing) {
    GdkCursor *cursor = gdk_cursor_new(CURSOR);
    gdk_window_set_cursor(cd->drawing->window, cursor);
    gdk_cursor_unref(cursor);
  }
}

static void update(CairoDesc *cd) {
  if (cd->drawing) {
    gtk_widget_queue_draw(cd->drawing);
    gdk_window_process_updates(cd->drawing->window, TRUE);
    gdk_flush();
  }
}

static int Cairo_HoldFlush(pDevDesc dd, int level) {
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  int prevHoldlevel = cd->holdlevel;

  cd->holdlevel = MAX(cd->holdlevel + level, 0);
  if (cd->holdlevel == 0) { /* hold released */
    update(cd);
    idle(cd);
  } else if (prevHoldlevel == 0) { /* first hold, flush and go busy */
    update(cd);
    busy(cd);
  }

  return cd->holdlevel;
}


static void Cairo_Mode(gint mode, pDevDesc dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;

  if (cd->holdlevel)
    return;
  
  if (cd->drawing) {
    if (!mode) {
      update(cd);
      idle(cd);
      //R_gtk_eventHandler(NULL);
    } else {
      busy(cd);
    }
  }
}

static SEXP Cairo_setPattern(SEXP pattern, pDevDesc dd) {
    return R_NilValue;
}

static void Cairo_releasePattern(SEXP ref, pDevDesc dd) {} 

static SEXP Cairo_setClipPath(SEXP path, SEXP ref, pDevDesc dd) {
    return R_NilValue;
}

static void Cairo_releaseClipPath(SEXP ref, pDevDesc dd) {}

static SEXP Cairo_setMask(SEXP path, SEXP ref, pDevDesc dd) {
    return R_NilValue;
}

static void Cairo_releaseMask(SEXP ref, pDevDesc dd) {}

Rboolean
configureCairoDevice(pDevDesc dd, CairoDesc *cd, double width, double height, double ps)
{
  gint ascent, descent, cw;
  PangoFont *success;
  PangoFontDescription *fontdesc;

  /* setup data structure */
    
  dd->deviceSpecific = (void *) cd;

  dd->close = Cairo_Close;
  dd->activate = Cairo_Activate;
  dd->deactivate = Cairo_Deactivate;
  dd->size = Cairo_Size;
  dd->newPage = Cairo_NewPage;
  dd->clip = Cairo_Clip;
  /* Next two are unused */
  dd->strWidth = Cairo_StrWidth;
  dd->text = Cairo_Text;
  dd->rect = Cairo_Rect;
  dd->circle = Cairo_Circle;
  dd->line = Cairo_Line;
  dd->polyline = Cairo_Polyline;
  dd->polygon = Cairo_Polygon;
#if R_GE_version > 6
  dd->path = Cairo_Path;
  dd->raster = Cairo_Raster;
  dd->cap = Cairo_Cap;
#endif
  dd->locator = Cairo_Locator;
  dd->mode = Cairo_Mode;
  dd->metricInfo = Cairo_MetricInfo;
#if R_GE_version < 7
  dd->getEvent = Cairo_GetEvent;
#else
  dd->eventHelper = Cairo_EventHelper;
#endif
  dd->hasTextUTF8 = TRUE;
  dd->wantSymbolUTF8 = TRUE;
  dd->strWidthUTF8 = Cairo_StrWidth;
  dd->textUTF8 = Cairo_Text;
#if R_GE_version >= 9
  dd->holdflush = Cairo_HoldFlush;
#endif

#if R_GE_version >= 9
  dd->haveTransparency = 2;
  dd->haveTransparentBg = 3;
  dd->haveRaster = 2;
  dd->haveCapture = cd->pixmap ? 2 : 1;
  dd->haveLocator = cd->drawing ? 2 : 1;
#endif

#if R_GE_version >= 13
  dd->setPattern      = Cairo_setPattern;
  dd->releasePattern  = Cairo_releasePattern;
  dd->setClipPath     = Cairo_setClipPath;
  dd->releaseClipPath = Cairo_releaseClipPath;
  dd->setMask         = Cairo_setMask;
  dd->releaseMask     = Cairo_releaseMask;
#endif

  dd->left = 0;
  dd->right = width;
  dd->bottom = height;
  dd->top = 0;
	
  // Setup font
  fontdesc = getBaseFont(cd);
  pango_font_description_set_size(fontdesc, PANGO_SCALE * ps);
  success = loadFont(fontdesc, cd);
  if (!success) {
    pango_font_description_free(fontdesc);
    Rprintf("Cannot find base font!\n");
    return(FALSE);
  }
  
  pango_context_set_font_description(getPangoContext(cd), fontdesc);
  font_metrics(fontdesc, cd, &cw, &ascent, &descent);
	
  pango_font_description_free(fontdesc);
  
  /* starting parameters */
  ps = 2 * (ps / 2);
  dd->startfont = 1; 
  dd->startps = ps;
  dd->startcol = R_RGB(0, 0, 0);
  dd->startfill = R_TRANWHITE;
  dd->startlty = LTY_SOLID; 
  dd->startgamma = 1;
	
  dd->cra[0] = cw;
  dd->cra[1] = ascent + descent;
	
  /* character addressing offsets */
  dd->xCharOffset = 0.4900;
  dd->yCharOffset = 0.3333;
  dd->yLineBias = 0.1;

  /* inches per raster unit */
  dd->ipr[0] = 1.0/POINTS_PER_INCH;
  dd->ipr[1] = 1.0/POINTS_PER_INCH;

  /* device capabilities */
  dd->canClip = TRUE;
  dd->canHAdj = 0; // maybe 1 or 2? 
  dd->canChangeGamma = FALSE; // not yet

  if (cd->drawing) {
    dd->canGenMouseDown = TRUE; /* can the device generate mousedown events */
    dd->canGenMouseMove = TRUE; /* can the device generate mousemove events */
    dd->canGenMouseUp = TRUE;   /* can the device generate mouseup events */
    if (cd->window)
      dd->canGenKeybd = TRUE;     /* can the device generate keyboard events */
  }

#if R_GE_version >= 13
  dd->deviceVersion = R_GE_definitions;
#endif

  dd->displayListOn = TRUE;
	
  /* finish */
  return TRUE;
}

Rboolean
createCairoDevice(pDevDesc dd, double width, double height, double ps, void *data) {
  /* device driver start */
	
  CairoDesc *cd;
	
  if(!(cd = createCairoDesc()))
    return FALSE;
	
  // need to create drawing area before we can configure the device
  if(!Cairo_Open(dd, cd, width, height, data)) {
    freeCairoDesc(dd);
    return FALSE;
  }

  return(configureCairoDevice(dd, cd, width, height, ps));
}
Rboolean
asCairoDevice(pDevDesc dd, double width, double height, double ps, void *data)
{
  CairoDesc *cd;
  gdouble left, right, bottom, top;
  gboolean success;

  if(!(cd = createCairoDesc()))
    return FALSE;

  if (width != -1) { // if width/height specified, we have custom context
    success = Cairo_OpenCustom(dd, cd, width, height, (cairo_t *)data);
  } else { // otherwise assume something from GTK
    if (GTK_IS_WIDGET(data))
      success = Cairo_OpenEmbedded(dd, cd, GTK_WIDGET(data));
    else success = Cairo_OpenOffscreen(dd, cd, GDK_DRAWABLE(data));
  }
  if (!success) {
    freeCairoDesc(dd);
    return FALSE;
  }
  
  Cairo_Size(&left, &right, &bottom, &top, dd);
	
  return(configureCairoDevice(dd, cd, right, bottom, ps));
}

typedef Rboolean (*CairoDeviceCreateFun)(pDevDesc , double width, 
                                         double height, double pointsize, void *data);

static  pDevDesc 
initCairoDevice(double width, double height, double ps, void *data, CairoDeviceCreateFun init_fun)
{
  pDevDesc dev;
  CairoDesc *cd;

  R_GE_checkVersionOrDie(R_GE_version);
  R_CheckDeviceAvailable();
  BEGIN_SUSPEND_INTERRUPTS {
    /* Allocate and initialize the device driver data */
    if (!(dev = (pDevDesc) calloc(1, sizeof(DevDesc))))
      return NULL;
    if (! init_fun (dev, width, height, ps, data)) {
      free(dev);
      PROBLEM  "unable to start device cairo" ERROR;
    }
    cd = (CairoDesc *)dev->deviceSpecific;
    /* wait to do this until the device is realized (if applicable) */
    if (!cd->drawing || GTK_WIDGET_REALIZED(cd->drawing))
      activateDevice(dev);
  } END_SUSPEND_INTERRUPTS;

  gdk_flush();
  return(dev);
}



// Width and Height are in inches
void
do_Cairo(double *in_width, double *in_height, double *in_pointsize, char **surface)
{
  char *vmax;
  double height, width, ps;

  /*    gcall = call; */
  vmax = vmaxget();
  width = *in_width;
  height = *in_height;
  if (width <= 0 || height <= 0) {
    PROBLEM "Cairo device: invalid width or height" ERROR;
  }
  ps = *in_pointsize;
 
  initCairoDevice(width*POINTS_PER_INCH, height*POINTS_PER_INCH, 
                  ps, surface, (CairoDeviceCreateFun)createCairoDevice);

  vmaxset(vmax);
  /*   return R_NilValue; */
}


SEXP
do_asCairoDevice(SEXP widget, SEXP pointsize, SEXP width, SEXP height)
{
  pDevDesc pd = initCairoDevice(asReal(width), asReal(height),
                                asReal(pointsize),
                                R_ExternalPtrAddr(widget),
                                asCairoDevice);
  return ScalarLogical(pd != NULL);
}
