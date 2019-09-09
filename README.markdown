Libinsane
---------

Libinsane is *the* library to access scanners on both Linux and Windows. It's
cross-platform, cross-programming languages, cross-scanners :-). It takes care
of all the quirks of all the platforms and scanners.

API is also designed so the application can display the scan as it goes if
it wishes to.

It has however some limitations:
- It is only designed to work with *scanners*, not webcams, not USB keys, etc
  (think paper-eaters only)
- TWAIN API or drivers may display some dialogs. Libinsane cannot prevent them.
- Full bed page scan only: Presence of the option to set the scan area cannot
  be guaranteed. You are advised to crop the image later in your own
  application (see [Paperwork](https://openpaper.work) for example).
- 24 bits color scans only (may be fixed later)
- On Windows (both with WIA2 or TWAIN), images are often rotated by 180°. This
  is because both APIs return the scan as BMP (DIB), and by default, BMP start
  by the bottom of the image. Since the application must be able to display the
  scan as it goes, Libinsane has to return the image rotated.


It is the successor of [Pyinsane2](https://gitlab.gnome.org/World/OpenPaperwork/pyinsane) but shares no code with it.

It is released under [LGPL v3+](https://www.gnu.org/licenses/lgpl-3.0.en.html).


- [Documentation](https://doc.openpaper.work/libinsane/latest/)
- [Bug tracker](https://gitlab.gnome.org/World/OpenPaperwork/libinsane/issues)
- [Mailing-list](https://gitlab.gnome.org/World/OpenPaperwork/paperwork/wikis/Contact)
