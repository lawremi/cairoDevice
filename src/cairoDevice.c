#include "cairoDevice.h"

#define CURSOR		GDK_CROSSHAIR		/* Default cursor */
#define MM_PER_INCH	25.4			/* mm -> inch conversion */

static PangoStyle  slant[]  = { PANGO_STYLE_NORMAL, PANGO_STYLE_OBLIQUE };
static PangoWeight weight[] = { PANGO_WEIGHT_NORMAL, PANGO_WEIGHT_BOLD };

CairoDesc *createCairoDesc() {
  return(g_new0(CairoDesc, 1));
}
void freeCairoDesc(NewDevDesc *dd) {
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
    cairo_show_page(cd->cr);
    cairo_destroy(cd->cr);
  }
  
  if (cd->filename)
    g_free(cd->filename);
	
  if (cd->surface)
    cairo_surface_destroy(cd->surface);
  
  g_free(cd);
}

static void activateDevice(NewDevDesc *dev)
{
  GEDevDesc *dd;
  CairoDesc *cd = (CairoDesc *)dev->deviceSpecific;
  GObject *obj = cd->drawing ? G_OBJECT(cd->drawing) : G_OBJECT(cd->pixmap);
  gsetVar(install(".Device"), mkString("Cairo"), R_NilValue);
  dd = GEcreateDevDesc(dev);
  GEaddDevice(dd);
  GEinitDisplayList(dd);
  if (obj) {
    SEXP devnum = ScalarInteger(ndevNumber(dev) + 1);
    R_PreserveObject(devnum);
    g_object_set_data_full(obj, ".devnum", devnum, (GDestroyNotify)R_ReleaseObject);
  }
}

/** need the pixel width/height in inches so we can convert 
    from R's inches to our pixels */
static double pixelWidth(void)
{
  double width, widthMM;
  width = gdk_screen_width();
  widthMM = gdk_screen_width_mm();
  return ((double)widthMM / (double)width) / MM_PER_INCH;
}
static double pixelHeight(void)
{
  double height, heightMM;
  height = gdk_screen_height();
  heightMM = gdk_screen_height_mm();
  return ((double)heightMM / (double)height) / MM_PER_INCH;
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

static void blank(NewDevDesc *dd) {
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  cairo_t *cr = cd->cr;
  gint width = cd->width, height = cd->height;
  
  if (GDK_IS_DRAWABLE(cd->pixmap))
    gdk_drawable_get_size(cd->pixmap, &width, &height);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_rectangle(cr, 0, 0, (gdouble)width, (gdouble)height);
  cairo_fill(cr);
  if (cd->drawing)
    gtk_widget_queue_draw(cd->drawing);
}

static gboolean initDevice(NewDevDesc *dd)
{
  CairoDesc *cd;
  GdkCursor *cursor;
  gdouble left, right, bottom, top;
	
  cd = (CairoDesc *) dd->deviceSpecific;
	
  Cairo_Size(&left, &right, &bottom, &top, dd);
  /*g_debug("%f %f %f %f" , left, right, bottom, top);*/
  dd->left = left;
  cd->width = dd->right = right;
  cd->height = dd->bottom = bottom;
  dd->top = top;
  
  /* set up crosshair cursor */
  if (cd->drawing) {
    cursor = gdk_cursor_new(GDK_CROSSHAIR);
    gdk_window_set_cursor(cd->drawing->window, cursor);
    gdk_cursor_unref(cursor);
  }
  
  if(cd->cr) {
    cairo_show_page(cd->cr);
    cairo_destroy(cd->cr);
  }
  if(cd->pixmap && cd->drawing)
    g_object_unref(cd->pixmap);
  
  /* create pixmap and cairo context for drawing, save default state */
  if(right > 0 && bottom > 0) {
    if (cd->drawing)
      cd->pixmap = gdk_pixmap_new(cd->drawing->window, right, bottom, -1);
    if (cd->surface)
      cd->cr = cairo_create(cd->surface);
    else cd->cr = gdk_cairo_create(cd->pixmap);
  //cairo_set_antialias(cd->cr, CAIRO_ANTIALIAS_NONE);
    //cd->pango = pango_cairo_font_map_create_context(
    //				PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
    //pango_cairo_context_set_resolution(cd->pango, 84);
    cairo_save(cd->cr);
    /* sync the size with the device */
  }
  
  return FALSE;
}

static void resize(NewDevDesc *dd)
{
    GEplayDisplayList(desc2GEDesc(dd));
}

static gboolean realize_event(GtkWidget *widget, NewDevDesc *dd)
{
  g_return_val_if_fail(dd != NULL, FALSE);
  initDevice(dd);
  return(FALSE);
}
static gboolean realize_embedded(GtkWidget *widget, NewDevDesc *dd)
{ /* different from above in that embedded device needs to be activated,
     since it wasn't when the device was created */
  g_return_val_if_fail(dd != NULL, FALSE);
  initDevice(dd);
  activateDevice(dd);
  return(FALSE);
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event, NewDevDesc *dd)
{
  CairoDesc *cd;
    
  g_return_val_if_fail(dd != NULL, FALSE);
  cd = (CairoDesc *)dd->deviceSpecific;
  g_return_val_if_fail(cd != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(cd->drawing), FALSE);

  if(cd->width != cd->drawing->allocation.width || cd->height != cd->drawing->allocation.height) 
    resize(dd); 
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

static void event_finish(NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  dd->onExit = NULL;
  dd->gettingEvent = FALSE;
  cd->event->active = FALSE;
  cd->event = NULL;
}
static void event_maybe_finish(NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (cd->event->result && cd->event->result != R_NilValue)
    event_finish(dd);
}
static void CairoEvent_onExit(NewDevDesc *dd)
{
  event_finish(dd);
}

/* converts the Gdk buttons to the button masks used by R's gevent.c */
#define R_BUTTON(button) pow(2, (button) - 1)
#define R_BUTTONS(state) (state) & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK) >> 8

static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (dd->gettingEvent) {
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseDown,
                                     R_BUTTON(event->button), event->x, event->y);
    event_maybe_finish(dd);
  }
  return(FALSE);
}

static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (dd->gettingEvent) {
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseUp,
                                     R_BUTTON(event->button), event->x, event->y);
    event_maybe_finish(dd);
  }
  return(FALSE);
}

static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (dd->gettingEvent) {
    cd->event->result = doMouseEvent(cd->event->rho, dd, meMouseMove,
                                     R_BUTTONS(event->state), event->x, event->y);
    event_maybe_finish(dd);
  }
  return(FALSE);
}

static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  if (dd->gettingEvent) {
    cd->event->result = doKeybd(cd->event->rho, dd, -1, gdk_keyval_name(event->keyval));
    event_maybe_finish(dd);
  }
  return(FALSE);
}

static SEXP Cairo_GetEvent(SEXP rho, const char *prompt)
{
  GEDevDesc *dd = GEcurrentDevice();
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

static void kill_cairo(NewDevDesc *dd)
{
    GEkillDevice(desc2GEDesc(dd));
}

static void unrealize_cb(GtkWidget *widget, NewDevDesc *dd) {
  g_return_if_fail(dd != NULL);
  if (dd->deviceSpecific) { /* make sure not called from freeCairoDesc */
    /* don't try to destroy the widget, since it's already being destroyed */
    ((CairoDesc *) dd->deviceSpecific)->drawing = NULL;
    kill_cairo(dd);
  }
}
static gint delete_event(GtkWidget *widget, GdkEvent *event, NewDevDesc *dd)
{
  g_return_val_if_fail(dd != NULL, FALSE);
  kill_cairo(dd);
  return TRUE;
}

static void setupWidget(GtkWidget *drawing, NewDevDesc *dd)
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

static Rboolean Cairo_OpenEmbedded(NewDevDesc *dd, CairoDesc *cd, GtkWidget *drawing)
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
static Rboolean Cairo_OpenOffscreen(NewDevDesc *dd, CairoDesc *cd, GdkDrawable *drawing)
{
  dd->deviceSpecific = cd;
  cd->pixmap = drawing;
  g_object_ref(G_OBJECT(cd->pixmap));
  // initialize
  initDevice(dd);		 
  return(TRUE);
}
static Rboolean Cairo_Open(NewDevDesc *dd, CairoDesc *cd,	double w, double h, 
                           const gchar **surface_info)
{	
  dd->deviceSpecific = cd;
	
  if (strcmp(surface_info[0], "screen")) {
    cairo_surface_t *surface;
    double width, height;
    if (!strcmp(surface_info[0], "png")) {
      width = w / pixelWidth();
      height = h / pixelHeight();
      surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
      cd->filename = g_strdup(surface_info[1]);
    } else {
      width = w*72.0;
      height = h*72.0;
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
    cd->width = width;
    cd->height = height;
    cd->surface = surface;
    return(TRUE);
  }

  cd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(cd->window), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(cd->window), 
                              w / pixelWidth(), h / pixelHeight());

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

static PangoFontDescription *getFont(CairoDesc *cd, R_GE_gcontext *gc)
{
  PangoFontDescription *fontdesc, *basedesc;
  gint size, face = gc->fontface;
    
  if (face < 1 || face > 5)
    face = 1;

  size = gc->cex * gc->ps + 0.5;
	
  basedesc = getBaseFont(cd);
  fontdesc = pango_font_description_new();
  if (face == SYMBOL_FONTFACE) {
    pango_font_description_set_family(fontdesc, "symbol");
  } else {
    if (strlen(gc->fontfamily))
      pango_font_description_set_family(fontdesc, gc->fontfamily);
    pango_font_description_set_weight(fontdesc, weight[(face-1)%2]);
    pango_font_description_set_style(fontdesc, slant[((face-1)/2)%2]);
  }
  pango_font_description_set_size(fontdesc, PANGO_SCALE * size);
  pango_font_description_merge(fontdesc, basedesc, FALSE);
	
  if (!loadFont(fontdesc, cd)) {
    char *strdesc = pango_font_description_to_string(fontdesc);
    fprintf(stderr, "Could not find font '%s', falling back to base font!\n", strdesc);
    g_free(strdesc);
    pango_font_description_free(fontdesc);
    fontdesc = basedesc;
  } else pango_font_description_free(basedesc);
	
  return(fontdesc);
}

static PangoLayout *layoutText(PangoFontDescription *desc, const char *str, 
			       CairoDesc *cd)
{
  PangoLayout *layout;
	
  //pango_cairo_update_context(cd->cr, cd->pango);
  //layout = pango_layout_new(cd->pango);
  if (cd->drawing)
    layout = gtk_widget_create_pango_layout(cd->drawing, NULL);
  else layout = pango_layout_new(gdk_pango_context_get());
  pango_layout_set_font_description(layout, desc);
  pango_layout_set_text(layout, str, -1);
  return(layout);
}

static void
text_extents(PangoFontDescription *desc, CairoDesc *cd, R_GE_gcontext *gc, 
             const gchar *text,
	     gint *lbearing, gint *rbearing, 
	     gint *width, gint *ascent, gint *descent)
{
  PangoLayout *layout;
  PangoRectangle rect;
	
  layout = layoutText(desc, text, cd);
  
  pango_layout_line_get_pixel_extents(pango_layout_get_line(layout, 0), NULL, &rect);

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
static void setLineType(cairo_t *cr, R_GE_gcontext *gc)
{
  cairo_line_cap_t cap = CAIRO_LINE_CAP_ROUND;
  cairo_line_join_t join = CAIRO_LINE_JOIN_ROUND;
  static double dashes[8];
  gint i;
	
  if(gc->lwd < 1)
    gc->lwd=1;
	
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

static void drawShape(cairo_t *cr, R_GE_gcontext *gc)
{
  if (gc->fill != NA_INTEGER) {
    setColor(cr, gc->fill);
    cairo_fill_preserve(cr);
  }
  if (gc->col != NA_INTEGER) {
    setColor(cr, gc->col);
    setLineType(cr, gc);
    cairo_stroke(cr);
  }
}

static void drawRect(cairo_t *cr, double x0, double y0, double x1, double y1, 
                     R_GE_gcontext *gc) 
{
  cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
  drawShape(cr, gc);
}

static double Cairo_StrWidth(char *str, R_GE_gcontext *gc, NewDevDesc *dd)
{
  gint width;
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  PangoFontDescription *desc = getFont(cd, gc);
  text_extents(desc, cd, gc, str, NULL, NULL, &width, NULL, NULL);
  pango_font_description_free(desc);
	
  return (double) width;
}

static void Cairo_MetricInfo(int c, R_GE_gcontext *gc,
                             double* ascent, double* descent, double* width, NewDevDesc *dd)
{
    gchar text[16], s[2];
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  gint iascent, idescent, iwidth;
  int Unicode = mbcslocale;
	
  PangoFontDescription *desc = getFont(cd, gc);
	
  if (!c) 
    font_metrics(desc, cd, &iwidth, &iascent, &idescent);
  else {
      if(c < 0) {c = -c; Unicode = 1;} 
      
      if (gc->fontface == 5) {
	  g_snprintf(s, 2, "%c", (gchar) c);
	  AdobeSymbol2utf8(text, s, 16);
      } else if(Unicode || c >= 128)
	  Rf_ucstoutf8(text, c);
      else
	  g_snprintf(text, 2, "%c", (gchar) c);
      text_extents(desc, cd, gc, text, NULL, NULL, &iwidth, &iascent, &idescent);
  }
	
  *ascent = iascent;
  *descent = idescent;
  *width = iwidth;
	
  //Rprintf("%f %f %f\n", *ascent, *descent, *width);
	
  pango_font_description_free(desc);
}

static void Cairo_Clip(double x0, double x1, double y0, double y1, NewDevDesc *dd)
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
                       NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  gint width = cd->width, height = cd->height;

  /* we have to use the allocation width/height if we have a widget, because
     the new backing pixmap is not created until later */
  if (cd->drawing) {
    if (GTK_WIDGET_MAPPED(cd->drawing)) {
      width = cd->drawing->allocation.width;
      height = cd->drawing->allocation.height;
    } else width = height = 1000; // hack to get R to draw before first exposure
  } else if (GDK_IS_DRAWABLE(cd->pixmap))
    gdk_drawable_get_size(cd->pixmap, &width, &height);

  *left = 0.0;
  *right = (gdouble)width;
  *top = 0.0;
  *bottom = (gdouble)height;
}

/* clear the drawing area */
static void Cairo_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = dd->deviceSpecific;
  gint width = cd->width, height = cd->height;
	
  initDevice(dd);
  
  if (!R_OPAQUE(gc->fill)) {
    blank(dd);
  }
	
  if (GDK_IS_DRAWABLE(cd->pixmap))
    gdk_drawable_get_size(cd->pixmap, &width, &height);
  
  drawRect(cd->cr, 0, 0, width, height, gc);
}

/** kill off the window etc
 */
static void Cairo_Close(NewDevDesc *dd)
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
setActive(NewDevDesc *dd, gboolean active)
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

static void Cairo_Activate(NewDevDesc *dd)
{
  setActive(dd, TRUE);
}

static void Cairo_Deactivate(NewDevDesc *dd)
{
  setActive(dd, FALSE);
}	

static void Cairo_Rect(double x0, double y0, double x1, double y1,
                       R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawRect(cd->cr, x0, y0, x1, y1, gc);
  cairo_restore(cd->cr);
}

static void drawCircle(cairo_t *cr, double x, double y, double r, R_GE_gcontext *gc)
{
  cairo_move_to(cr, x+r, y);
  cairo_translate(cr, x, y);
  cairo_arc(cr, 0, 0, r, 0, 2 * M_PI);
  //cairo_arc(cr, x, y, r, 0, 2 * M_PI);	
  drawShape(cr, gc);
}

static void Cairo_Circle(double x, double y, double r,
                         R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawCircle(cd->cr, x, y, r, gc);
  cairo_restore(cd->cr);
}

static void drawLine(cairo_t *cr, double x1, double y1, double x2, double y2,
                     R_GE_gcontext *gc)
{
  cairo_move_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
	
  setColor(cr, gc->col);
  setLineType(cr, gc);
  cairo_stroke(cr);
}

static void Cairo_Line(double x1, double y1, double x2, double y2,
                       R_GE_gcontext *gc, NewDevDesc *dd)
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

static void drawPolyline(cairo_t *cr, int n, double *x, double *y, R_GE_gcontext *gc)
{
  polypath(cr, n, x, y);
  setColor(cr, gc->col);
  setLineType(cr, gc);
  cairo_stroke(cr);
}

static void Cairo_Polyline(int n, double *x, double *y, 
                           R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  //g_debug("polyline");
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawPolyline(cd->cr, n, x, y, gc);
  cairo_restore(cd->cr);
}

static void drawPolygon(cairo_t *cr, int n, double *x, double *y, R_GE_gcontext *gc)
{
  polypath(cr, n, x, y);
  cairo_close_path(cr);
  drawShape(cr, gc);
}

static void Cairo_Polygon(int n, double *x, double *y, 
                          R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  g_return_if_fail(cd != NULL);
  g_return_if_fail(cd->cr != NULL);

  cairo_save(cd->cr);
  drawPolygon(cd->cr, n, x, y, gc);
  cairo_restore(cd->cr);
}

static void drawText(double x, double y, char *str, 
		     double rot, double hadj, CairoDesc *cd, R_GE_gcontext *gc)
{
  PangoLayout *layout;
  gint ascent, lbearing;
  cairo_t *cr = cd->cr;
	
  PangoFontDescription *desc = getFont(cd, gc);

  // x and y seem a little off, correcting...
  text_extents(desc, cd, gc, str, &lbearing, NULL, NULL, &ascent, NULL);
	
  cairo_move_to(cr, x, y);
  cairo_rotate(cr, -1*rot);
  cairo_rel_move_to(cr, -lbearing, -ascent);
  setColor(cr, gc->col);
	
  layout = layoutText(desc, str, cd);
  pango_cairo_show_layout(cr, layout);
	
  g_object_unref(layout);
  pango_font_description_free(desc);
}

static void Cairo_Text(double x, double y, char *str, 
                       double rot, double hadj, R_GE_gcontext *gc, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  double rrot = DEG2RAD * rot;
	
  //Rprintf("%s @ %f %f\n", str, x, y);
	
  cairo_save(cd->cr);
  drawText(x, y, str, rrot, hadj, cd, gc);
  cairo_restore(cd->cr);
}

static void locator_finish(NewDevDesc *dd)
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
				 NewDevDesc *dd)
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

static void CairoLocator_onExit(NewDevDesc *dd)
{
  locator_finish(dd);
}

static Rboolean Cairo_Locator(double *x, double *y, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
  CairoLocator *info;
  gboolean button1;
    
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(cd->drawing), FALSE);
    
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
    
  *x = (double) info->x;
  *y = (double) info->y;
  button1 = info->button1;

  g_free(info);
    
  if(button1)
    return TRUE;
  return FALSE;
}

static void Cairo_Mode(gint mode, NewDevDesc *dd)
{
  CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
  if (!mode && cd->drawing) {
    gtk_widget_queue_draw(cd->drawing);
    gdk_window_process_updates(cd->drawing->window,TRUE);
    gdk_flush();
    //R_gtk_eventHandler(NULL);
  }
}

Rboolean
configureCairoDevice(NewDevDesc *dd, CairoDesc *cd, double width, double height, double ps)
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
  dd->locator = Cairo_Locator;
  dd->mode = Cairo_Mode;
  dd->metricInfo = Cairo_MetricInfo;
  dd->getEvent = Cairo_GetEvent;
  dd->hasTextUTF8 = TRUE;
  dd->wantSymbolUTF8 = TRUE;
  dd->strWidthUTF8 = Cairo_StrWidth;
  dd->textUTF8 = Cairo_Text;

  dd->left = 0;
  dd->right = width;
  dd->bottom = height;
  dd->top = 0;
	
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
	
  /* starting parameters */
  ps = 2 * (ps / 2);
  dd->startfont = 1; 
  dd->startps = ps;
  dd->startcol = R_RGB(0, 0, 0);
  dd->startfill = R_TRANWHITE;
  dd->startlty = LTY_SOLID; 
  dd->startgamma = 1;
	
  pango_font_description_free(fontdesc);
	
  dd->cra[0] = cw;
  dd->cra[1] = ascent + descent;
	
  /* character addressing offsets */
  dd->xCharOffset = 0.4900;
  dd->yCharOffset = 0.3333;
  dd->yLineBias = 0.1;

  /* inches per raster unit */
  dd->ipr[0] = pixelWidth();
  dd->ipr[1] = pixelHeight();

  /* device capabilities */
  dd->canResizePlot = TRUE;
  dd->canChangeFont = TRUE;
  dd->canRotateText = TRUE;
  dd->canResizeText = TRUE;
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
    
  dd->displayListOn = TRUE;
	
  /* finish */
  return TRUE;
}

Rboolean
createCairoDevice(NewDevDesc *dd, double width, double height, double ps, void *data) {
  /* device driver start */
	
  CairoDesc *cd;
	
  if(!(cd = createCairoDesc()))
    return FALSE;
	
  // need to create drawing area before we can configure the device
  if(!Cairo_Open(dd, cd, width, height, data)) {
    freeCairoDesc(dd);
    return FALSE;
  }

  return(configureCairoDevice(dd, cd, width / pixelWidth(), height / pixelHeight(), ps));
}
Rboolean
asCairoDevice(NewDevDesc *dd, double width, double height, double ps, void *data)
{
  CairoDesc *cd;
  gdouble left, right, bottom, top;
  gboolean success;

  if(!(cd = createCairoDesc()))
    return FALSE;
	
  if (GTK_IS_WIDGET(data))
    success = Cairo_OpenEmbedded(dd, cd, GTK_WIDGET(data));
  else success = Cairo_OpenOffscreen(dd, cd, GDK_DRAWABLE(data));
  
  if (!success) {
    freeCairoDesc(dd);
    return FALSE;
  }
  
  Cairo_Size(&left, &right, &bottom, &top, dd);
	
  return(configureCairoDevice(dd, cd, right, bottom, ps));
}

typedef Rboolean (*CairoDeviceCreateFun)(NewDevDesc *, double width, 
                                         double height, double pointsize, void *data);

static  NewDevDesc *
initCairoDevice(double width, double height, double ps, void *data, CairoDeviceCreateFun init_fun)
{
  NewDevDesc *dev;
  CairoDesc *cd;

  R_CheckDeviceAvailable();
  BEGIN_SUSPEND_INTERRUPTS {
    /* Allocate and initialize the device driver data */
    if (!(dev = (NewDevDesc *) calloc(1, sizeof(NewDevDesc))))
      return NULL;
    /* Do this for early redraw attempts */
    dev->displayList = R_NilValue;
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
 
  initCairoDevice(width, height, ps, surface, (CairoDeviceCreateFun)createCairoDevice);

  vmaxset(vmax);
  /*   return R_NilValue; */
}


SEXP
do_asCairoDevice(SEXP widget, SEXP pointsize)
{
  void *drawing = R_ExternalPtrAddr(widget);
  double ps;
  SEXP ans = Rf_allocVector(LGLSXP, 1);

  ps = REAL(pointsize)[0];
  LOGICAL(ans)[0] = (initCairoDevice(-1, -1, ps, drawing, asCairoDevice) != NULL);

  return(ans);
}
