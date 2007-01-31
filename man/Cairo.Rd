\name{Cairo}
\alias{Cairo}
\title{
  Create a new Cairo-based graphics device
}
\description{
  \code{Cairo} initializes a new graphics device that uses the Cairo
  graphics library for rendering. The current implementation produces
  high-quality PNG bitmap files and high resolution PDF files with embedded
  fonts.

}
\usage{
Cairo(width = 640, height = 480, file="plot", type="png24", ps=14, 
bg = "transparent", backend="image")
}
\arguments{
  \item{width}{width of the plot area in pixels.}
  \item{height}{height of the plot area in pixels.}
  \item{file}{prefix of the file(s) that will be created. The final
    filename will be of the form
    <file>[<sequence>].<type-extention>. The only case when no extension
    is appended is when the file already has that extension.}
  \item{type}{file type used when producing the output. The actual
    files are created either on close (such as via dev.off()) or when a
    new page is requested. This version of Cario supports "png", a 24 
	bit RGB image, and "png24", 24 bit RGB with 8 bit Alpha channel.}
  \item{ps}{initial text size (in points).}
  \item{bg}{plot background color (can include alpha-component).}
  \item{backend}{Cairo back-end to be used. Currently only "image" is
    supported.}
}
\value{
  The (invisible) return value is NULL if the device couldn't be created
  or the raw device number if successful.
}
\section{Known issues}{
  \itemize{
	\item TrueType (and OpenType) fonts are supported when this package is compiled
	against a cairo graphics library configured with FreeType and Fontconfig support.
	\item R math symbols are supported,but require a TrueType "Symbol" font accessible
	to Cairo under that name.
  }
} 
\seealso{
  \code{\link{CairoFonts}}
}
\examples{
Cairo(800,600,file="plot.png")
plot(rnorm(100),rnorm(100),col=2)
dev.off() # creates a file "plot.png" with the above plot
}
\keyword{device}
