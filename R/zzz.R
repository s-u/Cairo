.First.lib <- function(libname, pkgname) {
  if (.Platform$OS.type=="windows") {
    lp<-gsub("/","\\\\",paste(libname,pkgname,"libs",sep="/"))
    cp<-strsplit(Sys.getenv("PATH"),";")
    if (! lp %in% cp) Sys.putenv(PATH=paste(lp,Sys.getenv("PATH"),sep=";"))
  }
  library.dynam("Cairo", pkgname, libname)
}
