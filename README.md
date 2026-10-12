## TO-DO 

Add error handling based on Kernel Flags 
Implenment API Client for future Panel Requests 
Look into multi-threading for Panel operations 
Redesign Process Panel (Looks kind of shitty)
Improve compile-time evaluation implementations
Look into perf overhead reports 
Improve context (AGENTS.md, etc)

## Photos

The photo panel occupies index 4 (the center). Put PNG photos in `photos/`, or run
`./build/libre-glass --photos /path/to/photos` to use another directory. The local
`photos/` directory is excluded from Git.

The default photo directory is resolved from the executable's location, so
launching from `build/` or another working directory still finds the project's
photos. The app prints its resolved photo directory at startup. An explicit
`--photos` path is resolved relative to your current working directory.

PNG Exif orientation is applied before fitting. Only portrait photos are shown.
Photos appear in filename order, fit the panel without cropping, and fade to the
next image every 30 seconds. A background loader decodes and resizes the current
and next images; directory changes are detected every two seconds. Unreadable
PNGs are skipped, and the current image stays visible until another is ready.

The build uses the system GdkPixbuf and GObject runtime libraries for PNG decoding
and resizing, plus pthreads. No Python process or image-generation script runs in
the mirror. On Debian/Ubuntu these runtime libraries are provided by
`libgdk-pixbuf-2.0-0` and `libglib2.0-0` (or its distribution-specific replacement).
