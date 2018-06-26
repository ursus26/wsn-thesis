import os
import subprocess
import numpy as np
import shlex


class Sim:
    def __init__(self, waf_path, ns3_path, script_name, verbose=True, nNodes=10,
                 debug=False, pcap=False, energy=10):
        self.waf_path = waf_path
        self.ns3_path = ns3_path
        self.script_name = script_name
        self.verbose = verbose
        self.debug = debug
        self.nNodes = nNodes
        self.pcap = pcap
        self.energy = energy

    def run(self, data_points):
        if self.debug:
            self.run_debug(data_points)
            return

        try:
            # Set the verbose level.
            stdout_action = subprocess.DEVNULL
            if self.verbose:
                stdout_action = None

            # Create the progam call and its arguments.
            script_args = " --command-template=\""
            script_args += "%" + "s"
            script_args += " --nNodes=" + str(self.nNodes)
            script_args += " --file=" + data_points.abs_path

            if self.pcap:
                script_args += " --pcap"

            if self.energy:
                script_args += " --energy=" + str(self.energy)

            # Closing part
            script_args = script_args + "\""

            run_args = " --run " + self.script_name
            program_call = self.waf_path + run_args + script_args
            program_call = shlex.split(program_call)

            subprocess.run(program_call,
                           stdin=subprocess.PIPE,
                           stdout=stdout_action,
                           cwd=self.ns3_path)

        except Exception as e:
            print("Error occured while running " + self.script_name)
            print("Error message: " + str(e))


    # TODO implement the dbg
    def run_debug(self, data_points):

        try:
            # Set the verbose level.
            stdout_action = subprocess.DEVNULL
            if self.verbose:
                stdout_action = None

            # Create the progam call and its arguments.
            script_args = " --command-template=\""
            script_args += "%" + "s"
            script_args += " --nNodes=" + str(self.nNodes)
            script_args = script_args + " --file=" + data_points.abs_path
            script_args = script_args + "\""

            run_args = " --run " + self.script_name
            program_call = self.waf_path + run_args + script_args
            program_call = shlex.split(program_call)

            subprocess.run(program_call,
                           stdin=subprocess.PIPE,
                           stdout=stdout_action,
                           cwd=self.ns3_path)

        except Exception as e:
            print("Error occured while running " + self.script_name)
            print("Error message: " + str(e))
