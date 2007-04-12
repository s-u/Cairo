.First.lib <- function(libname, pkgname) {
    ## add our libs to the PATH
    if (.Platform$OS.type=="windows") {
        .setenv <- if (exists("Sys.setenv")) Sys.setenv else Sys.putenv
        lp<-gsub("/","\\\\",paste(libname,pkgname,"libs",sep="/"))
        cp<-strsplit(Sys.getenv("PATH"),";")
        if (! lp %in% cp) .setenv(PATH=paste(lp,Sys.getenv("PATH"),sep=";"))
    }
    library.dynam("Cairo", pkgname, libname)
}

### in case we decide to keep the namespace ...
.onLoad <- .First.lib
