\name{Cairo}
\alias{Cairo}
\alias{CairoX11}
\alias{CairoPNG}
\alias{CairoPDF}
\alias{CairoSVG}
\alias{CairoPS}
\alias{CairoWin}
\alias{CairoJPEG}
\alias{CairoTIFF}
\title{
  Create a new Cairo-based graphics device
}
\description{
  \code{Cairo} initializes a new graphics device that uses the cairo
  graphics library for rendering. The current implementation produces
  high-quality PNG, JPEG, TIFF bitmap files, high resolution PDF files
  with embedded fonts, SVG graphics and PostScript files. It also
  provides X11 and Windows interactive graphics devices. Unlike other
  devices it supports all graphics features including alpha blending,
  anti-aliasing etc.

  \code{CairoX11}, \code{CairoPNG}, \code{CairoPDF}, \code{CairoPS} and
  \code{CairoSVG} are convenience wrappers of \code{Cairo} that take the
  same arguments as the corresponding device it replaces such as
  \code{X11}, \code{png}, \code{pdf}, etc. Use of the \code{Cairo}
  function is encouraged as it is more flexible than the wrappers.
}
\usage{
Cairo(width = 640, height = 480, file="", type="png", pointsize=12, 
      bg = "transparent", canvas = "white", units = "px", dpi = "auto",
      ...)

CairoX11(display=Sys.getenv("DISPLAY"), width = 7, height = 7,
         pointsize = 12, gamma = getOption("gamma"), bg = "transparent",
         canvas = "white", xpos = NA, ypos = NA, ...)
CairoPNG(filename = "Rplot\%03d.png", width = 480, height = 480,
         pointsize = 12, bg = "white",  res = NA, ...)
CairoJPEG(filename = "Rplot\%03d.jpeg", width = 480, height = 480,
         pointsize = 12, quality = 75, bg = "white", res = NA, ...)
CairoTIFF(filename = "Rplot\%03d.tiff", width = 480, height = 480,
         pointsize = 12, bg = "white", res = NA, ...)
CairoPDF(file = ifelse(onefile, "Rplots.pdf","Rplot\%03d.pdf"),
         width = 6, height = 6, onefile = TRUE, family = "Helvetica",
         title = "R Graphics Output", fonts = NULL, paper = "special",
         encoding, bg, fg, pointsize, pagecentre, ...)
CairoSVG(file = ifelse(onefile, "Rplots.svg", "Rplot\%03d.svg"),
         width = 6, height = 6, onefile = TRUE, bg = "transparent",
         pointsize = 12, ...)
CairoWin(width = 7, height = 7, pointsize = 12,
         record = getOption("graphics.record"),
         rescale = c("R", "fit", "fixed"), xpinch, ypinch, bg =
         "transparent", canvas = "white", gamma = getOption("gamma"),
         xpos = NA, ypos = NA, buffered = getOption("windowsBuffered"),
         restoreConsole = FALSE, ...)
CairoPS(file = ifelse(onefile, "Rplots.ps", "Rplot\%03d.ps"),
        onefile = TRUE, family, title = "R Graphics Output", fonts = NULL,
        encoding, bg, fg, width, height, horizontal, pointsize, paper,
        pagecentre, print.it, command, colormodel)
}
\arguments{
  \item{width}{width of the plot area (also see \code{units}).}
  \item{height}{height of the plot area (also see \code{units}).}
  \item{file}{name of the file to be created or connection to write
         to. Only PDF, PS and PNG types support connections. For X11
	 type \code{file} specifies the display name. If \code{NULL} or
	 \code{""} a reasonable default will be chosen which is
         \code{"plot.type"} for file-oriented types and value of the
         \code{DISPLAY} environment variable for X11. For image types
	 the file name can contain printf-style formatting expecting
	 one integer parameter which is the page number, such as
	 \code{"Rplot\%03d.png"}. The page numbers start at one.
	 The filename is expanded using \code{\link{path.expand}}.
       }
  \item{type}{output type. This version of Cario supports "png", "jpeg"
  and "tiff" bitmaps (png/tiff with transparent background), "pdf"
  PDF-file with embedded fonts, "svg" SVG-file, "ps" PostScript-file,
  "x11" X11 interactive window and "win" Windows graphics.
  A special type "raster" creates an image back-end that produces no
  actual output file but can be used in conjunction with any of
  \code{dev.capture()}, \code{grid.cap()} or \code{Cairo:::.image()}
  to create in-memory images.
  Depending on the support of various backends in cairo graphics some
  of the options may not be available for your system. See
  \code{\link{Cairo.capabilities}} function.}
  \item{pointsize}{initial text size (in points).}
  \item{canvas}{canvas color (must be opaque). The canvas is only used
    by devices that display graphics on a screen and the canvas is only
    visible only if bg is transparent.}
  \item{bg}{plot background color (can include alpha-component or be
  transparent alltogether).}
  \item{units}{units for of the \code{width} and \code{height}
         specifications. It can be any of \code{"px"} (pixels),
         \code{"in"} (inches), \code{"pt"} (points), \code{"cm"}
         (centimeters) or \code{"mm"} (millimeters).}
  \item{dpi}{DPI used for the conversion of units to pixels. If set to
    \code{"auto"} the DPI resolution will be determined by the
    back-end.}
  \item{...}{additional backend specific parameters (e.g. \code{quality}
         setting for JPEG (0..100), \code{compression} for TIFF
         (0,1=none, 5=LZW (default), 7=JPEG, 8=Adobe Deflate),
	 \code{locator} for a custom locator function in image back-ends)

	 The PDF back-end supports following additional arguments:
	 \code{author}, \code{subject}, \code{creator}, \code{keywords},
	 \code{create.date} and \code{modify.date}. If specified, all of
	 the above must be single strings. The dates must be in
	 PDF-defined format, you can use something like
	 \code{paste0("D:",gsub("[- :]","",.POSIXct(Sys.time(),"GMT"))),"Z")} 
	 to convert from \code{POSIXct} to PDF format. In addition, the
	 \code{version} argument (as documented in \code{\link{pdf}})
	 can be either a string or a scalar real number. However,
	 the cairographics library only supports values 1.4 and 1.5.
	 
	 All parameters
         listed below are defined by the other devices are are used by
         the wrappers to make it easier replace other devices by
         \code{Cairo}. They are described in detail in the documentation
         corresponding to the device that is being replaced.}
       \item{display}{X11 display, see \code{\link{X11}}}
       \item{gamma}{gamma correction}
       \item{xpos}{see \code{\link{X11}}}
       \item{ypos}{see \code{\link{X11}}}
       \item{filename}{same as \code{file} in \code{Cairo}}
       \item{res}{resolution in ppi, see \code{\link{png}}, will
         override \code{dpi} in \code{Cairo} if set to anything other
         than \code{NA} or \code{NULL}. Note that cairographics does not
         support tagging PNG output files with DPI so the raster image will
         be produced with the dpi setting, but readers may render it at
         some default dpi setting.}
       \item{quality}{quality of the jpeg, see \code{\link{jpeg}}}
       \item{onefile}{logical: if true (the default) allow multiple
         figures in one file (see \code{\link{pdf}}). false is currently
         not supported by vector devices}
       \item{family}{font family, see \code{\link{pdf}}}
       \item{title}{see \code{\link{pdf}}}
       \item{fonts}{see \code{\link{pdf}}, ignored, \code{Cairo}
         automatically detects and embeds fonts}
       \item{paper}{see \code{\link{pdf}} (ignored, \code{Cairo} uses device dimensions)}
       \item{encoding}{see \code{\link{pdf}} (ignored, \code{Cairo} uses
         native enconding except for symbols)}
       \item{fg}{see \code{\link{pdf}} (ignored)}
       \item{pagecentre}{see \code{\link{pdf}} (ignored, \code{Cairo}
         uses device dimensions and thus it is irrelevant)}
#ifdef windows
       \item{record}{see \code{\link{windows}} (ignored)}
       \item{rescale}{see \code{\link{windows}} (ignored)}
       \item{xpinch}{see \code{\link{windows}} (ignored)}
       \item{ypinch}{see \code{\link{windows}} (ignored)}
       \item{buffered}{see \code{\link{windows}} (ignored, \code{Cairo}
         always uses cache buffer)}
       \item{restoreConsole}{see \code{\link{windows}} (ignored)}
#endif
#ifndef windows
       \item{record}{Windows-specific, ignored on unix}
       \item{rescale}{Windows-specific, ignored on unix}
       \item{xpinch}{Windows-specific, ignored on unix}
       \item{ypinch}{Windows-specific, ignored on unix}
       \item{buffered}{Windows-specific, ignored on unix}
       \item{restoreConsole}{Windows-specific, ignored on unix}
#endif
       \item{horizontal}{see \code{\link{postscript}} (ignored)}
       \item{print.it}{see \code{\link{postscript}} (ignored)}
       \item{command}{see \code{\link{postscript}} (ignored)}
       \item{colormodel}{see \code{\link{postscript}} (ignored,
         \code{Cairo} always uses \code{RGB} or \code{ARGB})}

       
}
\value{
  The (invisible) return value is NULL if the device couldn't be created
  or a \code{Cairo} object if successful. The vaule of the object is the
  device number.
}
\section{Known issues}{
  \itemize{
    \item The X11 backend is quite slow. The reason is the cairographics
         implementation of the backend, so we can't do much about
         it. It should be possible to drop cairographics' Xlib
         backend entirely and use image backend copied into an X11
         window instead. We may try that in future releases.
   \item TrueType (and OpenType) fonts are supported when this package is compiled
	against a cairo graphics library configured with FreeType and
         Fontconfig support. Therefore make sure have a cairo graphics
         library with all bells and whistles to get a good result.
   \item R math symbols are supported, but require a TrueType "Symbol" font accessible
	to Cairo under that name.
  }
} 
\seealso{
  \code{\link{CairoFonts}}
}
\examples{
# very simple KDE
Cairo(600, 600, file="plot.png", type="png", bg="white")
plot(rnorm(4000),rnorm(4000),col="#ff000018",pch=19,cex=2) # semi-transparent red
dev.off() # creates a file "plot.png" with the above plot

# you can use any Cairo backend and get the same result
# vector, bitmap or on-screen
CairoPDF("plot.pdf", 6, 6, bg="transparent")
data(iris)
attach(iris)
plot(Petal.Length, rep(-0.03,length(Species)), xlim=c(1,7),
     ylim=c(0,1.7), xlab="Petal.Length", ylab="Density",
     pch=21, cex=1.5, col="#00000001", main = "Iris (yet again)",
     bg=c("#ff000020","#00ff0020","#0000ff20")[unclass(Species)])
for (i in 1:3)
  polygon(density(Petal.Length[unclass(Species)==i],bw=0.2),
    col=c("#ff000040","#00ff0040","#0000ff40")[i])
dev.off()

## remove the example files if not in an interactive session
if (!interactive()) unlink(c("plot.png","plot.pdf"))
}
\keyword{device}
