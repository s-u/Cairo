\name{Cairo.capture}
\alias{Cairo.capture}
\alias{Cairo.snapshot}
\title{
  Capture contents of an image backend or a display list snapshot.
}
\description{
  \code{Cairo.capture} is essentially the same as
  \code{dev.capture(native=TRUE)} with the exception that it works where
  \code{dev.capture} doesn't such as \code{onSave} callbacks.

  \code{Cairo.snapshot} is very similar to \code{recordPlot} except it
  also allows to retrieve the last snapshot.
}
\usage{
Cairo.capture(device = dev.cur())
Cairo.snapshot(device = dev.cur(), last=FALSE)
}
\arguments{
  \item{device}{device number or an object of the class \code{Cairo} (as
    obtained from the \code{\link{Cairo}} function).}
  \item{last}{logical, if \code{FALSE} then a new snapshot is created
    (exactly the same as \code{recordPlot()}), if \code{TRUE} then the last
    known snapshot is retrieved, if \code{NA} then a snapshot is created
    first, but if the display list is empty last snapshot is retrieved
    instead.}
}
\value{
  \code{Cairo.capture}: object of the class \code{nativeRaster}.

  \code{Cairo.snapshot}: object of the class \code{recordedplot}.
}
\author{
  Simon Urbanek
}
%\examples{
%}
\keyword{device}
