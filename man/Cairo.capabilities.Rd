\name{Cairo.capabilities}
\alias{Cairo.capabilities}
\title{
  Reports which output types are supported by this Cairo build
}
\description{
  \code{Cairo.capabilities} returns logical vector describing the
  capabilities of thsi particular Cairo build.
}
\usage{
Cairo.capabilities()
}
\details{
  The \code{Cairo} package provides multiple back-ends, such as images
  (PNG, JPEG, TIFF), vector graphics (PDF, PostScript, SVG) or display
  (X11, Windows). However, not all systems support all back-ends. The
  \code{Cairo.capabilities} function return a logical vector showing
  which capabilities are supported in this particular Cairo build.

  Note that the capabilities depend both on the libraries available in
  the system as well as the compiled-in modules in cairo graphics.
}
\seealso{
  \code{\link{Cairo}}
}
\keyword{device}
