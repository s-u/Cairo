### Copyright (C) 2004-2007 Simon Urbanek
### License: GPL v2

### mapping of supported type names to canonical type names
### as of 1.3-2 png/png24/png32 are the same (we don't support png8 anyway)
.supported.types <- c(png="png",png24="png",png32="png",jpeg="jpeg",jpg="jpeg",tiff="tiff",tif="tiff",
					  pdf="pdf",svg="svg",ps="ps",postscript="ps",x11="x11",xlib="x11",
					  win="win",win32="win",window="win",windows="win",w32="win")

Cairo <- function(width=640, height=480, file="", type="png", pointsize=12, bg="transparent", canvas="white", units="px", dpi="auto", ...) {
	ctype <- tolower(type)
	if (!ctype %in% names(.supported.types))
		stop("Unknown output type `",type,"'.")
	ctype <- .supported.types[ctype==names(.supported.types)]
	if (is.null(file) || !nchar(file))
		file <- if (ctype != 'x11') paste("plot.",ctype,sep='') else Sys.getenv("DISPLAY")

	if (typeof(file) == "character" && length(file) != 1)
		stop("file must be a character vector of length 1 or a connection")
	else if (inherits(file,"connection") && (summary(file)$opened != "opened" || summary(file)$"can write" != "yes"))
		stop("connection must be open and writeable")
	if (length(units)!=1 || ! units %in% c("px","pt","in","cm","mm"))
		stop("invalid unit (supported are px, pt, in, cm and mm)")
	if (any(dpi=="auto" || dpi=="")) dpi <- 0
	if (length(dpi)!=1 || !is.numeric(dpi) || dpi<0)
		stop("invalid dpi specification (must be 'auto' or a positive number)")
	dpi <- as.double(dpi)
	## unit multiplier: >0 mpl to get inches, <0 mpl to get device pixels
	umpl <- as.double(c(-1, 1/72, 1, 1/2.54, 1/25.4)[units==c("px","pt","in","cm","mm")])
	gdn<-.External("cairo_create_new_device", as.character(ctype), file, width, height, pointsize, bg, canvas, umpl, dpi, ..., PACKAGE="Cairo")
	par(bg=bg)
	invisible(structure(gdn,class=c("Cairo",paste("Cairo",toupper(ctype),sep='')),type=as.character(ctype),file=file))
}


###-------------- supporting functions -----------------

CairoFontMatch <- function(fontpattern="Helvetica",sort=FALSE,verbose=FALSE) {
	if (typeof(fontpattern) != "character")
		stop("fontname must be a character vector of length 1")

	if (typeof(sort) != "logical")
		stop("sort option must be a logical")

	if (typeof(verbose) != "logical")
		stop("verbose option must be a logical")

	invisible(.External("cairo_font_match",fontpattern,sort,verbose,PACKAGE="Cairo"))
}

CairoFonts <- function(regular="Helvetica:style=Regular",bold="Helvetica:style=Bold",italic="Helvetica:style=Italic",bolditalic="Helvetica:style=Bold Italic,BoldItalic",symbol="Symbol"){
	if (!is.null(regular) && typeof(regular) != "character")
		stop("regular option must be a character vector of length 1")
	if (!is.null(bold) && typeof(bold) != "character")
		stop("bold option must be a character vector of length 1")
	if (!is.null(italic) && typeof(italic) != "character")
		stop("italic option must be a character vector of length 1")
	if (!is.null(bolditalic) && typeof(bolditalic) != "character")
		stop("bolditalic option must be a character vector of length 1")
	if (!is.null(symbol) && typeof(symbol) != "character")
		stop("symbol option must be a character vector of length 1")
	invisible(.External("cairo_font_set",regular,bold,italic,bolditalic,symbol,PACKAGE="Cairo"))
}


###-------------- convenience wrapper functions -----------------

CairoX11 <- function(display=Sys.getenv("DISPLAY"), width = 7, height = 7, pointsize = 12,
					 gamma = getOption("gamma"), bg = "transparent", canvas = "white",
					 xpos = NA, ypos = NA) {
	Cairo(width, height, file=display, type='x11', pointsize=pointsize, bg=bg, units="in", dpi=72)
}

CairoPNG <- function(filename = "Rplot%03d.png", width = 480, height = 480,
					 pointsize = 12, bg = "white",  res = NA, ...) {
	Cairo(width, height, type='png', file=filename, pointsize=pointsize, bg=bg, ...)
}

CairoTIFF <- function(filename = "Rplot%03d.tiff", width = 480, height = 480,
					  pointsize = 12, bg = "white",  res = NA, ...) {
	Cairo(width, height, type='tiff', file=filename, pointsize=pointsize, bg=bg, ...)
}

CairoJPEG <- function(filename = "Rplot%03d.jpeg", width = 480, height = 480,
					  pointsize = 12, quality = 75, bg = "white", res = NA, ...) {
	Cairo(width, height, type='jpeg', file=filename, pointsize=pointsize, bg=bg, quality=quality, ...)
}

CairoPDF <- function(file = ifelse(onefile, "Rplots.pdf", "Rplot%03d.pdf"),
					 width = 6, height = 6, onefile = TRUE, family = "Helvetica",
					 title = "R Graphics Output", fonts = NULL, version = "1.1",
					 paper = "special", encoding, bg, fg, pointsize, pagecentre) {
	if (!onefile) stop("Sorry, PDF backend of Cairo supports onefile=TRUE only")
    if (missing(pointsize)) pointsize <- 12
    if (missing(bg)) bg <- "white"
	Cairo(width, height, file, "pdf", pointsize=pointsize, bg=bg, units="in")
}

CairoPS <- function(file = ifelse(onefile, "Rplots.ps", "Rplot%03d.ps"),
                    onefile = TRUE, family,
                    title = "R Graphics Output", fonts = NULL,
                    encoding, bg, fg,
                    width, height, horizontal, pointsize,
                    paper, pagecentre, print.it, command, colormodel) {
	if (!onefile) stop("Sorry, PostScript backend of Cairo supports onefile=TRUE only")
    if (missing(pointsize)) pointsize <- 12
    if (missing(bg)) bg <- "white"
    Cairo(width, height, file, "ps", pointsize=pointsize, bg=bg)
}   

CairoWin <- function(width = 7, height = 7, pointsize = 12,
					 record = getOption("graphics.record"),
					 rescale = c("R", "fit", "fixed"), xpinch, ypinch,
					 bg = "transparent", canvas = "white",
					 gamma = getOption("gamma"), xpos = NA, ypos = NA,
					 buffered = getOption("windowsBuffered"),
					 restoreConsole = FALSE) {
	Cairo(width, height, '', 'win', pointsize=pointsize, bg=bg, units="in")
}
