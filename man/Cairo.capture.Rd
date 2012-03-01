\name{Cairo.capture}
\alias{Cairo.capture}
\title{
  Capture contents of an image backend.
}
\description{
  \code{Cairo.capture} is essentially the same as
  \code{dev.capture(native=TRUE)} with the exception that it works where
  \code{dev.capture} doesn't such as \code{onSave} callbacks.
}
\usage{
Cairo.capture(device = dev.cur())
}
\arguments{
  \item{device}{device number or an object of the class \code{Cairo} (as
    obtained from the \code{\link{Cairo}} function).}
}
\value{
  Object of the class \code{nativeRaster}.
}
\author{
  Simon Urbanek
}
%\examples{
%}
\keyword{device}
