#!/usr/bin/env python3

# source ./activate_test_env.sh
# subprojects/libinsane-gobject/examples/test_hw.py <directory>

import os
import re
import sys

import PIL.Image

import gi
gi.require_version('Libinsane', '1.0')
from gi.repository import GObject  # noqa: E402
from gi.repository import Libinsane  # noqa: E402


class Logger(GObject.GObject, Libinsane.Logger):
    def do_log(self, lvl, msg):
        if lvl < Libinsane.LogLevel.WARNING:
            return
        print("{}: {}".format(lvl.value_nick, msg))


def get_devices(api):
    print("Looking for scan devices ...")
    dev_descs = api.list_devices(Libinsane.DeviceLocations.ANY)
    print("Found {} devices".format(len(dev_descs)))
    for dev in dev_descs:
        print("[{}] : [{}]".format(dev.get_dev_id(), dev.to_string()))
    devs = []
    for dev in dev_descs:
        print("")
        dev_id = dev.get_dev_id()
        devs.append(dev_id)
    return devs


def get_source(dev, source_name):
    print("Looking for scan sources ...")
    sources = dev.get_children()
    print("Found {} scan sources:".format(len(sources)))

    for src in sources:
        print("- {}".format(src.get_name()))

    for src in sources:
        if src.get_name().lower() == source_name.lower():
            print("Will use scan source {}".format(src.get_name()))
            return src

    for src in sources:
        if src.get_name().lower().startswith(source_name.lower()):
            print("Will use scan source {}".format(src.get_name()))
            return src

    raise Exception("Source '{}' not found".format(source_name))


def list_opts(item):
    opts = item.get_options()
    print("Options:")
    for opt in opts:
        try:
            print("- {}={} ({})".format(
                opt.get_name(), opt.get_value(), opt.get_constraint()
            ))
        except Exception as exc:
            print("Failed to read option {}: {}".format(
                opt.get_name(), str(exc)
            ))


def set_opt(item, opt_name, opt_values):
    print("Setting {} to {}".format(opt_name, opt_values))
    opts = item.get_options()
    opts = {opt.get_name(): opt for opt in opts}
    if opt_name not in opts:
        raise Exception("Option '{}' not found".format(opt_name))
    print("- Old {}: {}".format(opt_name, opts[opt_name].get_value()))
    print("- Allowed values: {}".format(opts[opt_name].get_constraint()))
    for opt_value in opt_values:
        if opt_value not in opts[opt_name].get_constraint():
            continue
        opts[opt_name].set_value(opt_value)
        opts = item.get_options()
        opts = {opt.get_name(): opt for opt in opts}
        print("- New {}: {}".format(opt_name, opts[opt_name].get_value()))
        return
    raise Exception(
        "Failed to set option {}: No value match constraint".format(opt_name)
    )


def raw_to_img(params, img_bytes):
    fmt = params.get_format()
    assert(fmt == Libinsane.ImgFormat.RAW_RGB_24)
    (w, h) = (
        params.get_width(),
        int(len(img_bytes) / 3 / params.get_width())
    )
    mode = "RGB"
    print("Mode: {} : Size: {}x{}".format(mode, w, h))
    return PIL.Image.frombuffer(mode, (w, h), img_bytes, "raw", mode, 0, 1)


def scan(source, output_file):
    session = source.scan_start()

    scan_params = session.get_scan_parameters()
    print("Expected scan parameters: {} ; {}x{} = {} bytes".format(
          scan_params.get_format(),
          scan_params.get_width(), scan_params.get_height(),
          scan_params.get_image_size()))

    try:
        page_nb = 0

        assert(not session.end_of_feed())

        img = []
        print("Scanning page --> {} ...".format(output_file))
        while not session.end_of_page():
            data = session.read_bytes(32 * 1024)
            data = data.get_data()
            img.append(data)
        img = b"".join(img)
        img = raw_to_img(scan_params, img)
        print("Saving page as {} ...".format(output_file))
        img.save(output_file)
        print("Page scanned".format(page_nb))

        assert(session.end_of_page())
        # TODO(Jflesch): HP ScanJet 4300C
        # assert(session.end_of_feed())
    finally:
        session.cancel()


def clean_filename(filename):
    return re.sub("[^0-9a-zA-Z.]", "_", filename)


def main():
    Libinsane.register_logger(Logger())

    if len(sys.argv) <= 1 or (sys.argv[1] == "-h" or sys.argv[1] == "--help"):
        print("Syntax: {} <output directory>".format(sys.argv[0]))
        sys.exit(1)

    output_dir = sys.argv[1]

    print("Will write the scan result into {}/".format(output_dir))
    os.mkdir(output_dir)

    api = Libinsane.Api.new_safebet()

    print("Looking for devices ...")

    devs = get_devices(api)
    if len(devs) <= 0:
        print("No device found")
        sys.exit(1)

    for dev_id in devs:
        print("Will use device {}".format(dev_id))
        for t in range(0, 3):
            print("- Test {}".format(t))
            dev = api.get_device(dev_id)
            try:
                print("Using device {}".format(dev.get_name()))
                output_file = clean_filename(dev.get_name() + ".jpeg")
                output_file = os.path.join(output_dir, output_file)

                print("Looking for source flatbed ...")
                src = get_source(dev, "flatbed")
                list_opts(src)

                # set the options
                set_opt(src, 'resolution', [150, 200, 300])

                print("Scanning ...")
                scan(src, output_file)
                print("Scan done")
            finally:
                dev.close()

    print("All scan done")


if __name__ == "__main__":
    main()
