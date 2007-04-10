\alias{asCairoDevice}
\name{asCairoDevice}
\title{Converting GTK+ widgets to Cairo graphics devices}
\description{Allows one to convert an arbitrary realized GTK+ widget (usually a 
  \code{\link[RGtk2]{GtkDrawingArea}}) or a \code{\link[RGtk2]{GdkDrawable}} 
  (like a \code{\link[RGtk2]{GdkPixmap}}) to a Cairo graphics 
device, so that R plot commands draw to the widget/drawable.}
\usage{asCairoDevice(widget, pointsize = 10)}
\arguments{
\item{widget}{The realized widget or drawable to which the R graphics are drawn}
\item{pointsize}{The default font size in the R plot}
}
\details{
  The main purpose of this function is to allow the user to 
  embed an R plot in a GTK user interface constructed with RGtk2. Please 
  see the examples in the RGtk2 package.
  
  Drawing to an offscreen \code{\link[RGtk2]{GdkPixmap}} is useful for
  buffering and transforming R graphics. 
  For example, one could copy the pixmap to a \code{\link[RGtk2]{GdkPixbuf}}
  and add a watermark before saving as a gif, png, jpg, etc.
}
\note{
  In previous versions of the package (pre 2.0), it was possible to call
  \code{asCairoDevice} on a widget before it was realized. This should not have been
  allowed, because it is impossible to draw to an unrealized widget. Many 
  published examples called \code{asCairoDevice} before
  realizing the widget, but they will now fail with an error. When calling
  \code{asCairoDevice} on a widget, first realize the widget, for example
  by adding the widget to a container where the top-level ancestor is visible.
}
  
\author{Michael Lawrence}
\references{
\url{http://www.ggobi.org/rgtk2}
\url{http://www.gtk.org/}
\url{http://www.cairographics.org/}
}
\keyword{device}
