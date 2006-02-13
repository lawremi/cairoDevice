.First.lib <- function(libname, pkgname)
{
    library.dynam("cairoDevice", pkgname, libname)
    # We prefer RGtk2, but we can fallback to basic event handling
	# using an input handler (on linux) - on windows we will use tcl_do hack
	if (!require("RGtk2"))
	{
		.C("loadGTK", PACKAGE="cairoDevice")
		.C("R_gtk_setEventHandler", PACKAGE="cairoDevice")
	}
}

.Last.lib <- function(libname, pkgname)
{
    devices <- dev.list()
    gtk.devices <- devices[names(devices)=="Cairo"]
    if(length(gtk.devices) > 0) {
        dev.off(gtk.devices)
    }
}
