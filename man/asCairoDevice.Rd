\alias{asCairoDevice}
\name{asCairoDevice}
\title{Converting GTK+ widgets to Cairo graphics devices}
\description{Allows one to convert an arbitrary GTK+ widget (usually a 
GtkDrawingArea) to a Cairo graphics device, so that R plot commands draw 
to the widget.}
\usage{asCairoDevice(widget, pointsize = 10)}
\arguments{
\item{widget}{The widget to which the R graphics are drawn}
\item{pointsize}{The default font size in the R plot}
}
\details{The main purpose of this function is to allow the user to 
embed an R plot in a GTK user interface constructed with RGtk2. Please 
see the examples in the RGtk2 package.}
\author{Michael Lawrence}
\references{
\url{http://www.ggobi.org/rgtk2}
\url{http://www.gtk.org/}
\url{http://www.cairographics.org/}
}
\keyword{device}
