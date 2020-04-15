# Cairo

**Cairo** is a graphics device for R which uses the cairo graphics library
to provide high-quality output in various formats including bitmap
(PNG, JPEG, TIFF), vector (PDF, PostScript, SVG) and on-screen (X11, Windows).
It supports alpha-blending (semi-transparent painting), anti-aliasing and font embedding.
Since the same engine is used for rendering on-screen and off-screen, is possible
to create output WYSIWYG-like output. In addition, it has capabilities to track
versions of the device content and provide custom locator function, making it
ideal for interactive and Web-based systems such as [RCloud](https://rcloud.social).

See the [files](https://RForge.net/Cairo/files) section for the latest release.

See also main [RForge page](https://RForge.net/Cairo) and [GitHub repository](https//github.com/s-u/Cairo).

## Installation

For the latest release use

```r
install.packages("Cairo")
```

For the latest development version, use

```r
install.packages("Cairo", repo="https://RForge.net")
```

[![CRAN](https://rforge.net/do/cransvg/Cairo)](https://cran.r-project.org/package=PKI)
[![RForge](https://rforge.net/do/versvg/Cairo)](https://RForge.net/Cairo)

## Building from this repository

This repostiroy does NOT include the actual package but rather source
files necessary to create one. In order to create a valid package you
need GNU `autoconf`. To build the package, run

```
sh mkdist
```

in the root of the repository which will create a tar-ball containing
the Cairo package.
