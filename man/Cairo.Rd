\alias{Cairo}
\alias{Cairo_pdf}
\alias{Cairo_ps}
\alias{Cairo_svg}
\alias{Cairo_png}
\name{Cairo}
\title{The Cairo Graphics Device}
\description{Open an R graphics device based on the Cairo vector graphics library}
\usage{
  Cairo(width = 7, height = 7, pointsize = 12,
        surface = c("screen", "png", "pdf", "ps", "svg"), filename = NULL)
  Cairo_pdf(filename, width = 7, height = 7, pointsize = 12)
  Cairo_ps(filename, width = 7, height = 7, pointsize = 12)
  Cairo_svg(filename, width = 7, height = 7, pointsize = 12)
  Cairo_png(filename, width = 7, height = 7, pointsize = 12)
}
\arguments{
  \item{width}{The (initial) width in inches}
  \item{height}{The (initial) height in inches}
  \item{pointsize}{The pointsize of the font}
  \item{surface}{One of \code{screen}, \code{pdf}, \code{ps}, \code{svg}, 
    or \code{png}, indicating the cairo backend to use (the output format).
    If missing, inferred from extension of the \code{filename} argument.
  }
  \item{filename}{The output filename (used only by file surfaces, not the screen).
  If a file surface is specified but no filename is given, the filename defaults
  to Rplots.\code{SURFACE} where \code{SURFACE} is the name of the current surface type.}
}
\details{
  Creates an R graphics device that draws to the specified Cairo surface. 
  By default, this will draw to a GTK+ window on the screen, but it also
  outputs to pdf, ps, png, and svg files, depending on the capabilities
  of your cairo installation.
  
  This functions the same as any other R graphics device. You may use the 
  conventional plot commands and expect essentially the same output, 
  except that everything is anti-aliased (similar to other vector-based 
  devices like Quartz). Alpha-blending is supported, as is enhanced interactivity
  via \code{\link[grDevices]{getGraphicsEvent}}. The device should work the
  same across all supported platforms (Mac, Windows, and Linux).
}
\seealso{\code{\link{asCairoDevice}} for embedding the device in an RGtk2 interface.}
\author{Michael Lawrence}
\references{
\url{http://www.cairographics.org/}
}
\examples{
Cairo()
plot(1:10)
dev.off()
}
\keyword{device}
