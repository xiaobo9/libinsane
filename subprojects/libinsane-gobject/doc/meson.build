## Doc

if build_machine.system() == 'linux'
    gnome.gtkdoc('libinsane-gobject',
        src_dir: include_directories('../src', '../include'),
        main_xml: 'libinsane_gobject-gtkdoc.xml',
        content_files: 'gtkdocentities.ent',
#    gobject_typesfile: 'libinsane.types',
        dependencies: libinsane_gobject_dep)
endif
