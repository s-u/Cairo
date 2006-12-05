Cairo <- function(width=640, height=480, file="plot", type="png24", ps=14, bg="transparent", backend="image") {
	if (is.null(file)) file<-"plot"

	if (type=="png" && bg=="transparent"){
		warning("Type png does not support transparency. Changing type to png24")
		type="png24"
	}
	gdn<-.External("cairo_create_new_device", as.character(type), as.character(file), width, height, ps, bg, PACKAGE="Cairo")
	par(bg=bg)
	invisible(gdn)
}

CairoFontMatch <- function(fontpattern="Helvetica",sort=FALSE,verbose=FALSE){

	if (typeof(fontpattern) != "character"){
		warning("fontname must be a character vector of length 1")
		return(invisible())
	}

	if (typeof(sort) != "logical"){
		warning("sort option must be a logical")
		return(invisible())
	}
	if (typeof(verbose) != "logical"){
		warning("verbose option must be a logical")
		return(invisible())
	}

	invisible(.External("cairo_font_match",fontpattern,sort,verbose,PACKAGE="Cairo"))
}

CairoFonts <- function(regular="Helvetica:style=Regular",bold="Helvetica:style=Bold",italic="Helvetica:style=Italic",bolditalic="Helvetica:style=Bold Italic,BoldItalic",symbol="Symbol"){
	if (!is.null(regular) && typeof(regular) != "character"){
		warning("regular option must be a character vector of length 1")
		return(invisible())
	}
	if (!is.null(bold) && typeof(bold) != "character"){
		warning("bold option must be a character vector of length 1")
		return(invisible())
	}
	if (!is.null(italic) && typeof(italic) != "character"){
		warning("italic option must be a character vector of length 1")
		return(invisible())
	}
	if (!is.null(bolditalic) && typeof(bolditalic) != "character"){
		warning("bolditalic option must be a character vector of length 1")
		return(invisible())
	}
	if (!is.null(symbol) && typeof(symbol) != "character"){
		warning("symbol option must be a character vector of length 1")
		return(invisible())
	}
	invisible(.External("cairo_font_set",regular,bold,italic,bolditalic,symbol,PACKAGE="Cairo"))
}
