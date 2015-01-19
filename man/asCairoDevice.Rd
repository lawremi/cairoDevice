\alias{asCairoDevice}
\name{asCairoDevice}
\title{Converting widgets and more to a Cairo graphics device}
\description{Allows one to convert an arbitrary GTK+ widget (usually a 
  \code{\link[RGtk2]{GtkDrawingArea}}), a \code{\link[RGtk2]{GdkDrawable}} 
  (like a \code{\link[RGtk2]{GdkPixmap}}), a \code{GtkPrintContext} or a
  \code{Cairo} context to a Cairo graphics device, so that R plot
  commands draw to the given target.}
\usage{asCairoDevice(widget, pointsize = 12, width = 500, height = 500)}
\arguments{
\item{widget}{The \code{GtkWidget}, \code{GdkDrawable},
  \code{GtkPrintContext}, or \code{Cairo} context, to which the R
  graphics are to be drawn}
\item{pointsize}{The default font size in the R plot}
\item{width}{Width in user coordinates, only used if \code{widget} is a
  Cairo context}
\item{height}{Height in user coordinates, only used if \code{widget} is a
  Cairo context}
}
\details{
  The main purpose of this function is to allow the user to 
  embed an R plot in a GTK user interface constructed with RGtk2. Please 
  see the examples in the RGtk2 package.
  
  Drawing to an offscreen \code{\link[RGtk2]{GdkPixmap}} is useful for
  buffering and transforming R graphics. 
  For example, one could copy the pixmap to a \code{\link[RGtk2]{GdkPixbuf}}
  and add a watermark before saving as a gif, png, jpg, etc.

  One may also pass a \code{Cairo} context as the \code{widget}
  parameter. This allows targeting arbitary Cairo surfaces. Note that
  the context state is for the most part not cleared before plotting, so
  transformations, etc, will remain in effect. Unlike Cairo, the R
  graphics system requires bounds to be specified, which may be done
  through the \code{width} and \code{height} parameters.

  As a convenience, \code{GtkPrintContext} is also accepted as the
  \code{widget} parameter. This makes it easy to redirect R graphics to a
  printer selected and configured through the GTK+ printing dialogs.
}

\author{Michael Lawrence}
\references{
\url{http://www.ggobi.org/rgtk2}
\url{http://www.gtk.org/}
\url{http://www.cairographics.org/}
}
\keyword{device}
