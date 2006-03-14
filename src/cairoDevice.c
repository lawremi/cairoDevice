#include "cairoDevice.h"

#define CURSOR		GDK_CROSSHAIR		/* Default cursor */
#define MM_PER_INCH	25.4			/* mm -> inch conversion */

static PangoStyle  slant[]  = { PANGO_STYLE_NORMAL, PANGO_STYLE_OBLIQUE };
static PangoWeight weight[] = { PANGO_WEIGHT_NORMAL, PANGO_WEIGHT_BOLD };

CairoDesc *createCairoDesc() {
	return(g_new0(CairoDesc, 1));
}
void freeCairoDesc(CairoDesc *cd) {
	if(cd->window)
		gtk_widget_destroy(cd->window);

	if(cd->cr)
		cairo_destroy(cd->cr);
	
    if(cd->pixmap)
		g_object_unref(cd->pixmap);
	
	g_free(cd);
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

static PangoFont *loadFont(PangoFontDescription *fontdesc, CairoDesc *cd)
{
    return(pango_context_load_font(gtk_widget_get_pango_context(cd->drawing), 
				fontdesc));
}

static PangoFontDescription *getBaseFont(CairoDesc *cd)
{
	return(pango_font_description_from_string("Verdana"));
}

static void blank(CairoDesc *cd) {
	cairo_t *cr = cd->cr;
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_rectangle(cr, 0, 0, cd->drawing->allocation.width, cd->drawing->allocation.height);
	cairo_fill(cr);
	gtk_widget_queue_draw(cd->drawing);
}

static gboolean initDisplay(NewDevDesc *dd)
{
	CairoDesc *cd;
	GdkCursor *cursor;
	gdouble left, right, bottom, top;
	
	cd = (CairoDesc *) dd->deviceSpecific;
	
	/* set up crosshair cursor */
	cursor = gdk_cursor_new(GDK_CROSSHAIR);
    gdk_window_set_cursor(cd->drawing->window, cursor);
	gdk_cursor_unref(cursor);
	
	/* sync the size with the device */
	Cairo_Size(&left, &right, &bottom, &top, dd);
    
	dd->left = left;
	cd->width = dd->right = right;
	cd->height = dd->bottom = bottom;
	dd->top = top;
	
	if(cd->cr)
		cairo_destroy(cd->cr);
	if(cd->pixmap)
	    g_object_unref(cd->pixmap);
	
	/* create pixmap and cairo context for drawing, save default state */
	if(right > 0 && bottom > 0) {
		cd->pixmap = gdk_pixmap_new(cd->drawing->window, right, bottom, -1);
	    cd->cr = gdk_cairo_create(cd->pixmap);
		//cd->pango = pango_cairo_font_map_create_context(
		//				PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
		//pango_cairo_context_set_resolution(cd->pango, 84);
		cairo_save(cd->cr);
	}
	
	return FALSE;
}

static void resize(NewDevDesc *dd)
{
	initDisplay(dd);

    GEplayDisplayList ((GEDevDesc*) Rf_GetDevice(Rf_devNumber((DevDesc*)dd)));
}

static gboolean realize_event(GtkWidget *widget, NewDevDesc *dd)
{
    g_return_val_if_fail(dd != NULL, FALSE);
    
    return(initDisplay(dd));
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
static gint delete_event(GtkWidget *widget, GdkEvent *event, NewDevDesc *dd)
{
    g_return_val_if_fail(dd != NULL, FALSE);

    Rf_KillDevice((DevDesc*) Rf_GetDevice(Rf_devNumber ((DevDesc*) dd)));
    
    return TRUE;
}

static Rboolean Cairo_OpenEmbedded(NewDevDesc *dd, CairoDesc *cd, GtkWidget *drawing)
{
	dd->deviceSpecific = cd;
	cd->drawing = drawing;
	// initialize
	if (!GTK_WIDGET_REALIZED(drawing))
		g_signal_connect(G_OBJECT (drawing), "realize",
		     G_CALLBACK (realize_event), dd);
	else initDisplay(dd);
	// hook it up for drawing
	g_signal_connect(G_OBJECT(drawing), "expose_event",
		     G_CALLBACK(expose_event), dd);
			 
	return(TRUE);
}

static Rboolean Cairo_Open(NewDevDesc *dd, CairoDesc *cd,	double w, double h)
{	
	dd->deviceSpecific = cd;
		
	cd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(cd->window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(cd->window), 
		w / pixelWidth(), h / pixelHeight());

    /* create drawingarea */
    cd->drawing = gtk_drawing_area_new();
    gtk_widget_set_events(cd->drawing, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
	
	/* connect to signal handlers, etc */
	g_signal_connect(G_OBJECT (cd->drawing), "realize",
		     G_CALLBACK (realize_event), dd);
	
	/* place and realize the drawing area */
	gtk_container_add(GTK_CONTAINER(cd->window), cd->drawing);
	
	/* connect to signal handlers, etc */
    g_signal_connect(G_OBJECT(cd->drawing), "expose_event",
		     G_CALLBACK(expose_event), dd);
    g_signal_connect(G_OBJECT(cd->window), "delete_event",
		     G_CALLBACK(delete_event), dd);
	
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

static PangoLayout *layoutText(PangoFontDescription *desc, const char *str, CairoDesc *cd)
{
	PangoLayout *layout;
    	char *utf8;
    	gsize bytes_written;
	
	//pango_cairo_update_context(cd->cr, cd->pango);
	//layout = pango_layout_new(cd->pango);
	layout = gtk_widget_create_pango_layout(cd->drawing, NULL);
	pango_layout_set_font_description(layout, desc);
	utf8 = g_locale_to_utf8(str, -1, NULL, &bytes_written, NULL);
    pango_layout_set_text(layout, utf8, -1);
	
	g_free(utf8);
	
	return(layout);
}

static void
text_extents(PangoFontDescription *desc, CairoDesc *cd, const gchar *text,
	      gint *lbearing, gint *rbearing, gint *width, gint *ascent, gint *descent)
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

	metrics = pango_context_get_metrics(gtk_widget_get_pango_context(cd->drawing), 
					desc, NULL);
	
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
	text_extents(desc, cd, str, NULL, NULL, &width, NULL, NULL);
	pango_font_description_free(desc);
	
	return (double) width;
}

static void Cairo_MetricInfo(int c, R_GE_gcontext *gc,
			    double* ascent, double* descent, double* width, NewDevDesc *dd)
{
    gchar text[2];
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	gint iascent, idescent, iwidth;
	
	PangoFontDescription *desc = getFont(cd, gc);
	
	if (!c) 
		font_metrics(desc, cd, &iwidth, &iascent, &idescent);
	else {
		g_snprintf(text, 2, "%c", (gchar) c);
		text_extents(desc, cd, text, NULL, NULL, &iwidth, &iascent, &idescent);
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

    *left = 0.0;
    *right =  cd->drawing->allocation.width;
    *bottom = cd->drawing->allocation.height;
    *top = 0.0;
}

/* clear the drawing area */
static void Cairo_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
    CairoDesc *cd;
	
    g_return_if_fail(dd != NULL);
    cd = (CairoDesc *) dd->deviceSpecific;
    g_return_if_fail(cd != NULL);
	g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
    g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));
    
	if (!R_OPAQUE(gc->fill)) {
		blank(cd);
    }
	
	drawRect(cd->cr, 0, 0, cd->drawing->allocation.width,
		cd->drawing->allocation.height, gc);
}

/** kill off the window etc
	we probably need to disconnect the expose signal from the drawing area,
	because if we're embedding the device the window is not destroyed
*/
static void Cairo_Close(NewDevDesc *dd)
{
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;

    freeCairoDesc(cd);
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

    devnum = Rf_devNumber((DevDesc*)dd) + 1;

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
    g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
	g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));
	
	cairo_save(cd->cr);
	drawRect(cd->cr, x0, y0, x1, y1, gc);
	cairo_restore(cd->cr);
}

static void drawCircle(cairo_t *cr, double x, double y, double r, R_GE_gcontext *gc)
{
	cairo_arc(cr, x, y, r, 0, 2 * M_PI);	
	drawShape(cr, gc);
}

static void Cairo_Circle(double x, double y, double r,
		       R_GE_gcontext *gc, NewDevDesc *dd)
{
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;

	g_return_if_fail(cd != NULL);
    g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
	g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));

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

    g_return_if_fail(cd != NULL);
    g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
	g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));

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
	
    g_return_if_fail(cd != NULL);
    g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
	g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));

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
    g_return_if_fail(GDK_IS_DRAWABLE(cd->pixmap));
	g_return_if_fail(GTK_IS_DRAWING_AREA(cd->drawing));
	g_return_if_fail(GDK_IS_WINDOW(cd->drawing->window));

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
	text_extents(desc, cd, str, &lbearing, NULL, NULL, &ascent, NULL);
	
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


typedef struct _Cairo_locator_info Cairo_locator_info;

struct _Cairo_locator_info {
    guint x;
    guint y;
    gboolean button1;
};

static void locator_button_press(GtkWidget *widget,
				 GdkEventButton *event,
				 gpointer user_data)
{
    Cairo_locator_info *info;

    info = (Cairo_locator_info *) user_data;

    info->x = event->x;
    info->y = event->y;
    if(event->button == 1)
		info->button1 = TRUE;
    else
		info->button1 = FALSE;

    gtk_main_quit();
}

static Rboolean Cairo_Locator(double *x, double *y, NewDevDesc *dd)
{
    CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
    Cairo_locator_info *info;
    guint handler_id;
    gboolean button1;

    info = g_new0(Cairo_locator_info, 1);

    /* Flush any pending events */
    while(gtk_events_pending())
		gtk_main_iteration();

    /* connect signal */
    handler_id = g_signal_connect(G_OBJECT(cd->drawing),
				  "button-press-event",
				  G_CALLBACK(locator_button_press), info);
    
    /* run the handler */
    gtk_main();

    *x = (double) info->x;
    *y = (double) info->y;
    button1 = info->button1;

    /* clean up */
    g_signal_handler_disconnect(G_OBJECT(cd->drawing), handler_id);
    g_free(info);

    if(button1)
		return TRUE;
    return FALSE;
}

static void Cairo_Mode(gint mode, NewDevDesc *dd)
{
	CairoDesc *cd = (CairoDesc *) dd->deviceSpecific;
	
	if (!mode) {
		gtk_widget_queue_draw(cd->drawing);
		gdk_flush();
	}
}

static void Cairo_Hold(NewDevDesc *dd)
{
}

Rboolean
configureCairoDevice(NewDevDesc *dd, CairoDesc *cd, double width, double height, double ps)
{
    gint ascent, descent, cw;
	PangoFont *success;
    PangoFontDescription *fontdesc;

	/* setup data structure */
    
    dd->deviceSpecific = (void *) cd;

    dd->newDevStruct = 1;

    dd->open = Cairo_Open;
    dd->close = Cairo_Close;
    dd->activate = Cairo_Activate;
    dd->deactivate = Cairo_Deactivate;
    dd->size = Cairo_Size;
    dd->newPage = Cairo_NewPage;
    dd->clip = Cairo_Clip;
    dd->strWidth = Cairo_StrWidth;
    dd->text = Cairo_Text;
    dd->rect = Cairo_Rect;
    dd->circle = Cairo_Circle;
    dd->line = Cairo_Line;
    dd->polyline = Cairo_Polyline;
    dd->polygon = Cairo_Polygon;
    dd->locator = Cairo_Locator;
    dd->mode = Cairo_Mode;
    dd->hold = Cairo_Hold;
    dd->metricInfo = Cairo_MetricInfo;

    dd->left = 0;
    dd->right = width / pixelWidth();
    dd->bottom = height / pixelHeight();
    dd->top = 0;
	
	fontdesc = getBaseFont(cd);
	pango_font_description_set_size(fontdesc, PANGO_SCALE * ps);
	success = loadFont(fontdesc, cd);
	if (!success) {
		pango_font_description_free(fontdesc);
		Rprintf("Cannot find base font!\n");
		return(FALSE);
	}
	pango_context_set_font_description(gtk_widget_get_pango_context(cd->drawing), 
		fontdesc);
    font_metrics(fontdesc, cd, &cw, &ascent, &descent);
	
	/* starting parameters */
    ps = 2 * (ps / 2);
    dd->startfont = 1; 
    dd->startps = ps;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startfill = NA_INTEGER;
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
	if(!Cairo_Open(dd, cd, width, height)) {
		freeCairoDesc(cd);
		return FALSE;
    }
	
	return(configureCairoDevice(dd, cd, width, height, ps));
}
Rboolean
asCairoDevice(NewDevDesc *dd, double width, double height, double ps, void *data)
{
	GtkWidget *drawing = GTK_WIDGET(data);
	CairoDesc *cd;
	gdouble w = width, h = height;

	if(!(cd = createCairoDesc()))
		return FALSE;
	
	if(!Cairo_OpenEmbedded(dd, cd, drawing)) {
		freeCairoDesc(cd);
		return FALSE;
    }
	
	w = drawing->allocation.width * pixelWidth();
	h = drawing->allocation.height * pixelHeight();
	
	return(configureCairoDevice(dd, cd, w, h, ps));
}

typedef Rboolean (*CairoDeviceCreateFun)(NewDevDesc *, double width, 
	double height, double pointsize, void *data);

static  GEDevDesc *
initCairoDevice(double width, double height, double ps, void *data, CairoDeviceCreateFun init_fun)
{
    GEDevDesc *dd;
    NewDevDesc *dev;

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
		gsetVar(install(".Device"), mkString("Cairo"), R_NilValue);
		dd = GEcreateDevDesc(dev);
		//dd->newDevStruct = 1;
		Rf_addDevice((DevDesc*) dd);
		GEinitDisplayList(dd);
    } END_SUSPEND_INTERRUPTS;

    gdk_flush();

    return(dd);
}


void
do_Cairo(double *in_width, double *in_height, double *in_pointsize)
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
 
    initCairoDevice(width, height, ps, NULL, (CairoDeviceCreateFun)createCairoDevice);

    vmaxset(vmax);
    /*   return R_NilValue; */
}


SEXP
do_asCairoDevice(SEXP widget, SEXP pointsize)
{
    GtkWidget *drawing = GTK_WIDGET(R_ExternalPtrAddr(widget));
    double ps;
    SEXP ans = Rf_allocVector(LGLSXP, 1);

    ps = REAL(pointsize)[0];
    LOGICAL(ans)[0] = (initCairoDevice(-1, -1, ps, drawing, asCairoDevice) != NULL);

    return(ans);
}
