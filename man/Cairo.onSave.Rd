\name{Cairo.onSave}
\alias{Cairo.onSave}
\title{
  Cairo callbacks
}
\description{
  \code{Cairo.onSave} set the \code{onSave} callback which allows R code
  to be run when Cairo finalizes a page (either due to a new page being
  created or by the device being closed). The callback expects
  \code{function(device, page)} where \code{device} will be the device
  number and \code{page} is the currently finished page number (starting
  at 1).
}
\usage{
Cairo.onSave(device = dev.cur(), onSave)
}
%- maybe also 'usage' for other objects documented here.
\arguments{
  \item{device}{device number or \code{Cairo} object (as returned by the
    \code{\link{Cairo}} function)}
  \item{onSave}{function that will replace the current callback or
    \code{NULL} to remove the current callback}
}
%\details{
%}
\value{
  The old callback being replaced or \code{NULL} if there was none.
}
%\references{
%}
\author{
Simon Urbanek
}
\note{
  The function \code{onSave} will be evaluated in the global environment
  and no error checking is done, so you must make sure to catch errors,
  otherwise the behavior is undefined (and may included crashing R or
  other bad things).
}
\seealso{
  \code{\link{Cairo}}
}
\examples{
if (require(png, quietly=TRUE)) {
  dev <- Cairo(800, 600, type='raster')
  Cairo.onSave(dev, function(dev, page)
    .GlobalEnv$png <- writePNG(Cairo.capture(dev))
  )
  plot(1:10, col=2)
  dev.off()
  str(png)
}
}
\keyword{device}
