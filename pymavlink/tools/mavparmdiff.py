#!/usr/bin/env python3
'''
compare two MAVLink parameter files
'''

from pymavlink import mavparm

from argparse import ArgumentParser
parser = ArgumentParser(description=__doc__)
parser.add_argument("file1", metavar="FILE1")
parser.add_argument("file2", metavar="FILE2")
parser.add_argument("-t",
                    help="use tabs delimiter between columns for the output",
                    default=False,
                    action='store_true',
                    dest='use_tabs')
parser.add_argument("--full-diff",
                    help="include volatile and similar parameters",
                    default=True,
                    action='store_false',
                    dest='use_excludes')
parser.add_argument("--hide-only1",
                    help="hide params only in first file",
                    default=False,
                    action='store_true')
parser.add_argument("--hide-only2",
                    help="hide params only in second file",
                    default=False,
                    action='store_true')
parser.add_argument("--param-docs",
                    help="show the meaning of parameter values (needs MAVProxy, and 'param download' to have been run)",
                    default=False,
                    action='store_true')
parser.add_argument("--vehicle",
                    help="vehicle type for --param-docs (eg. Plane, Copter, Rover, Sub, Blimp, Heli, AntennaTracker)",
                    default=None)
parser.add_argument("--param-xml",
                    help="apm.pdef.xml file to use for --param-docs instead of the downloaded one",
                    default=None)

args = parser.parse_args()


def get_value_info():
    '''return a callable(name, value) giving the meaning of a parameter value'''
    if args.vehicle is None and args.param_xml is None:
        print("--param-docs needs --vehicle or --param-xml")
        return None
    try:
        from MAVProxy.modules.lib.param_help import ParamHelp
    except ImportError as ex:
        print("--param-docs needs MAVProxy installed: %s" % ex)
        return None
    ph = ParamHelp()
    if args.param_xml is not None:
        ph.param_use_xml_filepath(args.param_xml)
    else:
        ph.vehicle_name = args.vehicle
    if ph.param_help_tree(True) is None:
        return None

    def value_info(name, value):
        info = ph.param_info(name, value)
        if info is None:
            return None
        return str(info)
    return value_info


value_info = get_value_info() if args.param_docs else None

file1 = args.file1
file2 = args.file2

p1 = mavparm.MAVParmDict()
p2 = mavparm.MAVParmDict()
p1.load(file2, use_excludes=args.use_excludes)
print("FILE1: %s" % file1)
print("FILE2: %s" % file2)
p1.diff(file1, use_excludes=args.use_excludes, use_tabs=args.use_tabs,
        show_only1=not args.hide_only1,
        show_only2=not args.hide_only2,
        header=True,
        value_info=value_info)
