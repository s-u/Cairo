\name{CairoFonts}
\alias{CairoFonts}
\title{
  Set the fonts used for all Cairo graphics devices.
}
\description{
  \code{CairoFonts} initializes the fonts used for Cairo graphics devices.
}
\usage{
CairoFonts(
	regular="Helvetica:style=Regular",
	bold="Helvetica:style=Bold",
	italic="Helvetica:style=Italic",
	bolditalic="Helvetica:style=Bold Italic,BoldItalic",
	symbol="Symbol", usePUA=TRUE
)
}
\arguments{
  \item{regular}{character; fontconfig pattern for the 'plain text' font.}
  \item{bold}{character; fontconfig pattern for the 'bold face' font.}
  \item{italic}{character; fontconfig pattern for the 'italic' font.}
  \item{bolditalic}{character; fontconfig pattern for the 'bold italic' font.}
  \item{symbol}{character; fontconfig pattern for the 'symbol' font.}
  \item{usePUA}{logical; if \code{FALSE} then symbols are using regular
    Unicode code points (supported by regular fonts), otherwise Private
    Unicode Area (PUA) of symbols is used (typically better propulated
    by specialized symbol fonts). R 4.0.0 or higher is required for
    \code{usePUA=FALSE}.}
}
\details{
	This function sets the fonts for Cairo graphics devices globally; previously opened
	Cairo graphics devices will also use these fonts. The argument names correspond to 
	the five values of the graphical parameter 'font', i.e. regular is 1, bold is 2, italic 
	is 3, etc.

	For an explanation of fontconfig patterns, see \code{\link{CairoFontMatch}}.
}
\section{Known issues}{
  \itemize{
    \item This function is only available when the cairo graphics library is configured
    with FreeType and Fontcofig support.
    \item R math symbols are supported, but require a "Symbol" font with
    the Adobe Symbol Encoding unless \code{usePUA=FALSE} is used
    (available in R 4.0.0 or higher only).
  }
}
\seealso{
  \code{\link{CairoFontMatch}},
  \code{\link{Cairo}},
  \code{\link{par}},
}
\examples{
\dontrun{
#
# The following fontconfig patterns define the free truetype fonts 
# available in the debian package 'ttf-freefont'.
# 
# Freesans is very similar to Helvetica
CairoFonts(
	regular="FreeSans:style=Medium",
	bold="FreeSans:style=Bold",
	italic="FreeSans:style=Oblique",
	bolditalic="FreeSans:style=BoldOblique"
)
}}
\keyword{device}
