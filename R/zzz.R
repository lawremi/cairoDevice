.onLoad <- function(libname, pkgname)
{
    #library.dynam("cairoDevice", pkgname, libname)
		.C("loadGTK", PACKAGE="cairoDevice")
		.C("R_gtk_setEventHandler", PACKAGE="cairoDevice")
    options(device="Cairo")
}

.Last.lib <- function(libname, pkgname)
{
    devices <- dev.list()
    gtk.devices <- devices[names(devices)=="Cairo"]
    if(length(gtk.devices) > 0) {
        dev.off(gtk.devices)
    }
}
