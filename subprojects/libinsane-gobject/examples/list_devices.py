#!/usr/bin/env python3

# source ./activate_test_env.sh
# subprojects/libinsane-gobject/examples/list_devices.py

import gi
gi.require_version('Libinsane', '0.1')
from gi.repository import GObject  # noqa: E402

#! [Logger]
from gi.repository import Libinsane  # noqa: E402


class ExampleLogger(GObject.GObject, Libinsane.Logger):
    def do_log(self, lvl, msg):
        if lvl <= Libinsane.LogLevel.WARNING:
            return
        print("{}: {}".format(lvl.value_nick, msg))


def main():
    Libinsane.register_logger(ExampleLogger())
    #! [Logger]
    api = Libinsane.Api.new_safebet()
    print("Looking for devices ...")
    devs = api.list_devices(Libinsane.DeviceLocations.ANY)
    print("Found {} devices".format(len(devs)))
    for d in devs:
        try:
            dev = api.get_device(d.get_dev_id())
            print("|")
            print("|-- {} ({} ; {})".format(
                d.get_dev_id(), d.to_string(), dev.get_name()
            ))
            try:
                for child in dev.get_children():
                    print("|   |-- {}".format(child.get_name()))
            finally:
                dev.close()
        except Exception as exc:
            print("ERROR: failed to open device {}: {}".format(
                d.get_dev_id(), str(exc)
            ))


if __name__ == "__main__":
    main()
