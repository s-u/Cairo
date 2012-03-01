\name{Cairo.serial}
\alias{Cairo.serial}
\title{
  Check for changes in the graphics state of Cairo devices.
}
\description{
  \code{Cairo.serial} returns an integer that is increased with every
  plotting operation on the device. This allows user code to determine
  whether any new content has been added to the device since it was last
  checked.
}
\usage{
Cairo.serial(device = dev.cur())
}
\arguments{
  \item{device}{device number or an object of the class \code{Cairo} (as
    obtained from the \code{\link{Cairo}} function).}
}
%\details{
%}
\value{
  Integer value.
}
\author{
  Simon Urbanek
}
\note{
  The integer value overflows to 0 at 2^31. Typically only equality
  should be checked and for such it is extremely unlikely that the state
  has changed yet the serial value is the same due to overflow.
}
%\examples{
%}
\keyword{device}
