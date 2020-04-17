Libinsane
---------

Libinsane is *the* library to access scanners on both Linux and Windows.

Its main features are:
- Cross-platform: tested on Linux and Windows,
  [by CI](https://gitlab.gnome.org/World/OpenPaperwork/libinsane/pipelines) and
  [by users](https://openpaper.work/scanner_db/).
- [Cross-API](https://doc.openpaper.work/libinsane/latest/libinsane/scan_apis.html):
  supports Sane (Linux), WIA2 (Windows) and TWAIN (Windows)
- Cross-scanners: takes care of all the
  [quirks](https://doc.openpaper.work/libinsane/latest/libinsane/workarounds.html)
  of all the platforms and all the scanners to provide a
  [consistent behaviour](https://doc.openpaper.work/libinsane/latest/libinsane/behavior_normalizations.html)
  everywhere.
- Cross-programming languages:
  [Libinsane-GObject](https://doc.openpaper.work/libinsane/latest/libinsane-gobject/index.html)
  provides bindings for
  [many programming languages](https://wiki.gnome.org/Projects/GObjectIntrospection/Users)
  (Python, Java, Ruby, etc) thanks to
  [GObject Introspection](https://gi.readthedocs.io/en/latest/).
- Returns the scan as it goes: whenever possible, the image returned by the
  scanner is returned to the application as the scan goes.
- Very few runtime dependencies: Libinsane itself is a pure C library with
  the strict minimum of runtime dependencies. Only Libinsane-Gobject depends
  on the [GLib](https://developer.gnome.org/glib/).

However it has some limitations:
- It is only designed to work with *scanners*, not webcams, not USB keys, etc
  (think paper-eaters only)
- TWAIN API or drivers may display some dialogs. Libinsane cannot prevent them.
- Full page scan only: Presence of the options to set the scan area and their
  consistency cannot be guaranteed. You are advised to crop the image later
  in your own application (see [Paperwork](https://openpaper.work) for example).
- On Windows (both with WIA2 or TWAIN), images are often rotated by 180°. This
  is because both APIs return the scan as a BMP (DIB), and by default, BMPs start
  by the bottom of the image. Libinsane returns the top of the image first
  (as Sane does). Since we want applications to be able to display the
  scan as it goes, Libinsane has to rotate the image.
- We do our best to support as many scanners and drivers as possible. However not
  all the scanners in the world are supported (nor even supportable). You can have
  a look at the [Scanner database](https://openpaper.work/scannerdb/) to see the
  scanners that are known to work and those that don't.

It is the successor of [Pyinsane2](https://gitlab.gnome.org/World/OpenPaperwork/pyinsane) but shares no code with it.

It is released under [LGPL v3+](https://www.gnu.org/licenses/lgpl-3.0.en.html).

- [Documentation](https://doc.openpaper.work/libinsane/latest/)
- [Forum](https://forum.openpaper.work/)
- [Bug tracker](https://gitlab.gnome.org/World/OpenPaperwork/libinsane/issues)
- [Scanner database](https://openpaper.work/scannerdb/)
- [Donate](https://www.patreon.com/openpaper) (I also accept hardware donations
  to fix specific issues)

Related third-party projects:
- [lisgo](https://github.com/foenixx/lisgo) Go bindings for Libinsane
