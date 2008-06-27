.onLoad <- function(libname, pkgname)
{
  dll <- try(library.dynam("cairoDevice", pkgname, libname))
  if (is.character(dll)) {
    warning("Failed to load cairoDevice dynamic library:", dll)
    .install_system_dependencies()
    return()
  }
  
  #library.dynam("cairoDevice", pkgname, libname)
  if (!.C("loadGTK", success = logical(1), PACKAGE="cairoDevice")$success)
    message("Note: R session is headless; Cairo device not initialized")
  else {
    .C("R_gtk_setEventHandler", PACKAGE="cairoDevice")
    options(device="Cairo")
  }

  # register device as being interactive
  deviceIsInteractive("Cairo")
}

.Last.lib <- function(libname, pkgname)
{
    devices <- dev.list()
    gtk.devices <- devices[names(devices)=="Cairo"]
    if(length(gtk.devices) > 0) {
        dev.off(gtk.devices)
    }
}

.install_system_dependencies <- function()
{
  windows_config <- list(
    source = F,
    gtk_url = "http://downloads.sourceforge.net/gladewin32/gtk-2.12.9-win32-2.exe",
    installer = function(path) {
      shell(path)
    }
  )
  
  darwin_config <- list(
    source = F,
    gtk_url = "http://r.research.att.com/gtk2-runtime.dmg", 
    installer = function(path) {
      system(paste("hdiutil attach ", path, sep=""))
      system("open '/Volumes/GTK+ 2.10.11 run-time/gtk2-runtime.pkg'")
      system("hdiutil detach '/Volumes/GTK+ 2.10.11 run-time'")
    }
  )
  
  unix_config <- NULL
  
  gtk_web <- "http://www.gtk.org"
  
  install_system_dep <- function(dep_name, dep_url, dep_web, installer)
  {
    if (!interactive()) {
      cat("Please install ", dep_name, " from ", dep_url)
      return()
    }
    choice <- menu(paste(c("Install", "Do not install"), dep_name), T, 
      paste("Need", dep_name, "?"))
    if (choice == 1) {
      path <- file.path(tempdir(), basename(dep_url))
      if (download.file(dep_url, path, mode="wb") > 0)
        stop("Failed to download ", dep_name)
      installer(path)
    }
    message(paste("Learn more about", dep_name, "at", dep_web))
  }
  
  install_all <- function() {
    if (.Platform$OS.type == "windows")
      config <- windows_config
    else if (length(grep("darwin", R.version$platform))) 
      config <- darwin_config
    else config <- unix_config
    
    if (is.null(config))
      stop("Sorry this platform is not yet supported by the automatic GTK+ installer.",
        "Please install GTK+ manually, if necessary. See: ", gtk_web)
    
    install_system_dep("GTK+", config$gtk_url, gtk_web, config$installer)
  }
  
  install_all()
  
  message("PLEASE RESTART R BEFORE TRYING TO LOAD THE PACKAGE AGAIN")
}
