.surfaces <- c("screen", "png", "pdf", "ps", "svg")

Cairo <- function(width = 7, height = 7, pointsize = 10, surface = "screen", filename = NULL)
{
  if (missing(surface) && !missing(filename))
    surface <- tolower(gsub(".*\\.", "", basename(filename)))
  if (!(surface %in% .surfaces))
    stop("Surface must be one of: ", paste(.surfaces, collapse=", "))
  if (missing(filename) && surface != "screen")
    filename <- paste("Rplots", surface, sep=".")
  surface_info <- c(surface, filename)
  .C("do_Cairo", as.numeric(width), as.numeric(height), as.numeric(pointsize),
    as.character(surface_info), PACKAGE="cairoDevice")
  return(invisible(TRUE))
}

Cairo_png <- function(filename, width = 7, height = 7, pointsize = 10)
{
  Cairo(width, height, pointsize, "png", filename)
}
Cairo_pdf <- function(filename, width = 7, height = 7, pointsize = 10)
{
  Cairo(width, height, pointsize, "pdf", filename)
}
Cairo_ps <- function(filename, width = 7, height = 7, pointsize = 10)
{
  Cairo(width, height, pointsize, "ps", filename)
}
Cairo_svg <- function(filename, width = 7, height = 7, pointsize = 10)
{
  Cairo(width, height, pointsize, "svg", filename)
}

asCairoDevice <- function(widget, pointsize = 10)
{
    if (!require("RGtk2"))
       stop("asGtkDevice requires the RGtk package")
    if(!inherits(widget, "GtkDrawingArea") && !inherits(widget, "GdkDrawable")) {
        stop("Object being used as a Cairo Device must be a GtkDrawingArea or GdkDrawable")
    }

    .Call("do_asCairoDevice", widget, as.numeric(pointsize), PACKAGE="cairoDevice")
}
