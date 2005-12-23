Cairo <- function(file="plot", type="png", width=400, height=300, ps=12, bg="white") {
  invisible(.External("gdd_create_new_device", as.character(type), as.character(file), width, height, ps, bg, PACKAGE="Cairo"))
}

