\name{Cairo}
\alias{Cairo}
\title{
  Create a new Cairo-based graphics device
}
\description{
  \code{Cairo} initializes a new graphics device that uses the Cairo
  graphics library for rendering. The current implementation uses
  "image" back-end of Cairo to produce high-quality PNG bitmap files.
}
\usage{
Cairo(width = 640, height = 480, file="plot", type="png", ps=12, bg = "white", backend="image")
}
\arguments{
  \item{width}{width of the plot area in pixels.}
  \item{height}{height of the plot area in pixels.}
  \item{file}{prefix of the file(s) that will be created. The final
    filename will be of the form
    <file>[<sequence>].<type-extention>. The only case when no extension
    is appended is when the file already has that extension.}
  \item{type}{file format used when producing the output. The actual
    files are created either on close (such as via dev.off()) or when a
    new page is requested. This version of Cario supports "png" format
    only.}
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
    \item Support for math symbols is untested and requires a
    "Symbol" font accessible to Cairo under that name.
  }
} 
\examples{
Cairo(800,600,file="plot.png")
plot(rnorm(100),rnorm(100),col=2)
dev.off() # creates a file "plot.png" with the above plot
}
\keyword{device}
