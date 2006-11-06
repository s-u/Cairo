Cairo <- function(width=640, height=480, file="plot", type="png", ps=12, bg="white", backend="image") {
  if (is.null(file)) file<-"plot"
  gdn<-.External("cairo_create_new_device", as.character(type), as.character(file), width, height, ps, bg, PACKAGE="Cairo")
  par(bg=bg)
  invisible(gdn)
}
