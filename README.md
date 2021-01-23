# Introduction

Sauklaue is a simplistic stylus note-taking application optimized for use with an external graphics tablet, especially for online teaching. It is also useful for giving slideshow presentations. It runs on Linux systems using the X11 windowing system.

In modern classrooms, there are usually multiple blackboard panels, and one alternates between them. Only one blackboard panel is erased at a time.

Slideshow presentations usually suffer from a lack of a second projector displaying the previous slide. A brief moment of thought is enough and you don't read the last sentence on a slide before it disappears. Of course, hand-written online teaching suffers from the same problem if you use a single page that is periodically cleared. Common formats of computer screens suggest showing two portrait pages side by side. This is what Sauklaue does.

Furthermore, the writing area on an external graphics tablet can be mapped to just one of the two displayed pages (the _current_ page). (Lay down your graphics tablet in portrait orientation.) This way, you can write twice as big (in area) as you would otherwise, which generally improves handwriting.

Documents can be exported to PDF and PDF files can be imported.

Otherwise, the application is extremely simplistic: For example, there is only a fixed set of pen colors and sizes. You can only add hand-written notes, no vector graphics or text. You currently can't even set the page size in an easy way! (It's set to A4.)

![Screenshot](docs/screenshot1.png?raw=true)

![Screenshot](docs/screenshot2.png?raw=true)

# Similar applications

I've previously used [Xournal++](<https://xournalpp.github.io/>) for online teaching. It works well and has many more features than Sauklaue, but lacks some of the main features of Sauklaue: Mapping an external graphics tablet to the currently active page, simultaneously showing nonadjacent pages, support for latex beamer presentations with `\pause`.

Xournal++ is based on [Xournal](<https://sourceforge.net/projects/xournal/>).

# Installation

## As a package

There is an [AUR package](https://aur.archlinux.org/packages/sauklaue/) for Arch Linux.

## From source

### Dependencies

1. A recent C++ compiler supporting C++17
1. Cmake
1. Qt 5
1. Cap'n Proto
1. Cairo
1. Cairomm (C++ wrapper for the cairo library)
1. Poppler-glib
1. X11 library (libX11)
1. X11 input extension library (libXi)
1. KConfig library
1. KConfigWidgets library
1. KGuiAddons library

#### Arch Linux

On Arch Linux, you need the following packages:

```
qt5-base capnproto cairomm poppler-glib hicolor-icon-theme libx11 libxi kconfig kconfigwidgets kguiaddons
```

#### Ubuntu

On Ubuntu 20.04, you need the following packages:

```
g++ cmake qtbase5-dev libcairomm-1.0-dev capnproto libcapnp-dev libpoppler-glib-dev libkf5config-dev libkf5configwidgets-dev libkf5guiaddons-dev libxi-dev
```

### Build instructions

```
git clone https://github.com/fagu/sauklaue.git
cd sauklaue
mkdir build
cd build
cmake ..
make
sudo make install
```

# Usage

`sauklaue` or `sauklaue gui` opens the graphical user interface. To map an external graphics tablet to the correct screen area, make sure to set it up in the preferences dialog.

`sauklaue export` allows you to convert a file to pdf on the command line. Use `sauklaue export -h` for help.

# Tips and tricks

## Erasing

To erase, use the right "mouse button". (Map an appropriate button on the stylus to the right mouse button.)

## Laser pointer

To point at a particular location, you can use a temporary red "laser pointer" with the middle "mouse button".

## Showing earlier pages

Normally, the two columns are "linked": They are forced to always display consecutive pages of the document.

With the "unlink views" button (below the pen size buttons), you can unlink the columns. You can then independently change the displayed page in both columns. This can for example be useful when you want to remind your audience of something mentioned earlier, while still showing the most recent page.

## Slideshow presentations

If you want to give a presentation created with the LaTeX beamer package and "animations" (`\pause`, `\uncover`, etc.): Try including it with "Insert PDF file (by label)". You should see every frame in its first state (before the first `pause` command). Use "Next PDF page" to uncover more text on the current page (up to the next `pause`). Anything written on the current slide will remain visible when you do this.

## Setting your own page size

This is not implemented yet, but as a workaround, you can import a PDF file with empty pages of the correct size.

# Troubleshooting

## In a setup with multiple screens, the tablet is not mapped to the correct region

Make sure that before starting Sauklaue, the tablet is mapped to the bounding box of all screens, not just to one screen. (For example, for the XP-PEN tablet, load the driver and make sure to set the mapping to "AllScreen". Sauklaue will manipulate the mapping assuming the internal setting is "AllScreen".)

# Privacy warnings
If you import a pdf file, the entire file will be part of the sauklaue file, even if you later delete some (or all) of the pdf's pages.

Just like any pen, the eraser tool only paints over the things you wrote previously. It doesn't actually erase any information.

The above disclaimers remain true if you export to pdf. It might still be possible to recover some information about pages in the original pdf file that were later deleted. The eraser strokes could easily be removed from the pdf file, revealing the text written below.
