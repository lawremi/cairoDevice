Cairo <- function(width = 7, height = 7, pointsize = 10)
{
    .C("do_Cairo", as.numeric(width), as.numeric(height), as.numeric(pointsize), 
		PACKAGE="cairoDevice")
    return(invisible(TRUE))
}

asCairoDevice <- function(widget, pointsize = 10)
{
    if (!require("RGtk2"))
       stop("asGtkDevice requires the RGtk package")
    if(!inherits(widget, "GtkDrawingArea")) {
        stop("Widget being used as a Cairo Device must be a GtkDrawingArea")
    }

    .Call("do_asCairoDevice", widget, as.numeric(pointsize), PACKAGE="cairoDevice")
}
