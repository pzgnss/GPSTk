#!/usr/env python

import sys
import os
import argparse
import unittest

import gpstk

args = None

def run_unit_tests():
    """A function to run unit tests without using the argument parsing of
    unittest.main() """

    parser = argparse.ArgumentParser(description="Test app for ctest")
    parser.add_argument('-v,--verbose', dest='verbose', action='count',
                        help="Increase the amount of output generated by the program")
    parser.add_argument('-o,--output_dir', dest='output_dir', metavar='dir',
                        help="Path to output directory")
    parser.add_argument('-i,--input_dir', dest='input_dir', metavar='dir',
                        help="Path to root of input data directory")
    global args
    args=parser.parse_args()

    runner=unittest.TextTestRunner()

    # Find all Test classes in the parent script
    script=os.path.basename(sys.argv[0])
    dir=os.path.dirname(sys.argv[0])
    isuite = unittest.TestLoader().discover(dir, pattern=script)

    rc = runner.run(isuite)
    sys.exit(len(rc.failures))
