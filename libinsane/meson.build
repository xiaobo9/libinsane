if host_machine.system() == 'windows'
    message('Target OS: Microsoft Windows (Mingw)')
    add_project_arguments('-DOS_WINDOWS', '-DCOMPILER_GCC', language: 'c')
else
    message('Target OS: Unix-like')
    add_project_arguments('-DOS_LINUX', '-DCOMPILER_GCC', language: 'c')
endif

subdir('include')
subdir('src')
subdir('tests')
