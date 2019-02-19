PYTHON ?= python3
VERBOSE ?=
PREFIX ?= /usr/local

SRCS = $(wildcard src/libinsane/*.c)
HEADERS = $(wildcard include/libinsane/*.h)

all: build check test

build: build_c build_py

install: install_py install_c

uninstall: uninstall_py uninstall_c

build_py:

build/build.ninja:
	mkdir -p build
	(cd build && meson --werror --warnlevel 3 --prefix=${PREFIX} ..)

build_c: build/build.ninja
	(cd build && ninja)

version:

doc: build/build.ninja
	# Libinsane doc
	(cd build && ninja subprojects/libinsane/doc/doc_out)
	# Libinsane-gobject doc (Meson 0.37.1 || Meson 0.47.1)
	(cd build ; ninja libinsane-gobject-doc || ninja libinsane-gobject@@libinsane-gobject-doc)
	rm -rf doc/build
	mkdir -p doc/build
	mv build/doc/html doc/build/libinsane
	mv build/subprojects/libinsane-gobject/doc/html doc/build/libinsane-gobject
	cp doc/index.html doc/build
	echo "Documentation is available in doc/build/"

check: build_c
	! command -v sparse || python3 ./check_sparse.py build/compile_commands.json
	# Debian / Ubuntu
	(cd build ; ! command -v run-clang-tidy-4.0.py || ! (run-clang-tidy-4.0.py | grep warning 2>&1))
	(cd build ; ! command -v run-clang-tidy-7 || ! (run-clang-tidy-7 | grep warning 2>&1))
	# Fedora
	(cd build ; [ ! -f /usr/share/clang/run-clang-tidy.py ] || ! (/usr/share/clang/run-clang-tidy.py | grep warning 2>&1))

test: build/build.ninja
	(cd build && ninja test)

test_hw:
	rm -rf test_hw_out
	subprojects/libinsane-gobject/tests/test_hw.py test_hw_out

linux_exe:

windows_exe:

release:
ifeq (${RELEASE}, )
	@echo "You must specify a release version ($(MAKE) release RELEASE=1.2.3)"
else
	@echo "Will release: ${RELEASE}"
	@echo "Checking release is in ChangeLog ..."
	grep ${RELEASE} ChangeLog
	@echo "Checking release is in meson.build ..."
	grep ${RELEASE} meson.build
	@echo "Checking release is in subprojects/libinsane/meson.build ..."
	grep ${RELEASE} subprojects/libinsane/meson.build
	@echo "Checking release is in subprojects/libinsane-gobject/meson.build ..."
	grep ${RELEASE} subprojects/libinsane-gobject/meson.build
	@echo "Releasing ..."
	git tag -a ${RELEASE} -m ${RELEASE}
	git push origin ${RELEASE}
	@echo "All done"
endif

clean:
	rm -rf build
	rm -rf doc/build
	rm -rf subprojects/libinsane-gobject/generated
	mkdir -p subprojects/libinsane-gobject/generated
	touch subprojects/libinsane-gobject/generated/.notempty

install_py:

install_c: build/build.ninja
	(cd build && ninja install)

uninstall_py:

uninstall_c:
	(cd build && ninja uninstall)

help:
	@echo "make build || make build_c || make build_py"
	@echo "make check"
	@echo "make doc"
	@echo "make help: display this message"
	@echo "make install || make install_py"
	@echo "make release"
	@echo "make test"
	@echo "make uninstall || make uninstall_py"

.PHONY: \
	build \
	build_c \
	build_py \
	check \
	doc \
	linux_exe \
	windows_exe \
	help \
	install \
	install_c \
	install_py \
	release \
	test \
	uninstall \
	uninstall_c \
	uninstall_py \
	version
