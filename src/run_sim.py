import os
import subprocess
import argparse
import numpy as np
import matplotlib.pyplot as plt
import sim

# File paths.
NS3_PATH = os.path.dirname(os.path.realpath(__file__)) + "/../ns3/ns-3.28"
WAF_PATH = NS3_PATH + "/waf"

# Scripts that are run in the simulation.
SCRIPTS = ['wsn_leach']

# Command line arguments
VERBOSE = False
DEBUG = False
PCAP = False
NODES = 10


class Points:
    def __init__(self):
        self.point_list = np.array([])
        self.file_name = "points.out"
        self.abs_path = ""

    def generate_points(self, nPoints=0, xmax=100, ymax=100):
        coords = np.random.random((nPoints + 1, 3)).astype(np.float32)
        # coords = np.random.randint(0, xmax, size=(nPoints + 1, 3))

        for i in np.arange(1, nPoints + 1):
            coords[i, 0] = coords[i, 0] * xmax
            coords[i, 1] = coords[i, 1] * ymax
            coords[i, 2] = 0

        # Base station coordinate.
        coords[0, 0] = xmax / 2.0
        coords[0, 1] = ymax / 2.0
        coords[0, 2] = 0.0

        self.point_list = np.around(coords)

    def plot(self):
        plt.figure()
        plt.scatter(self.point_list[1:, 0], self.point_list[1:, 1], s=30, color="blue", zorder=3)
        plt.scatter(self.point_list[:1, 0], self.point_list[:1, 1], s=30, color="red", zorder=3)
        plt.xlabel("x")
        plt.ylabel("y")
        plt.legend(["Sensors", "Base station"], fontsize=14)
        plt.margins(x=0.1, y=0.1)
        plt.grid(True)
        plt.show()

    def save(self, file_name="points.out"):
        self.file_name = file_name
        np.savetxt(self.file_name, self.point_list, delimiter=',', fmt='%.6f')
        self.abs_path = os.path.abspath(self.file_name)


def parse_args():
    """
    Parse command line arguments for this script.
    """
    global VERBOSE
    global NODES
    global DEBUG
    global PCAP

    # Init the argument parser.
    parser = argparse.ArgumentParser()

    # Add commandline arguments.
    parser.add_argument("-v", "--verbose", help="increase output verbosity",
                        action="store_true")
    parser.add_argument("-d", "--debug", help="Debug ns3 option", action="store_true")
    parser.add_argument("-p", "--pcap", help="Enable pacture capture", action="store_true")
    parser.add_argument("-n", "--nNodes", help="Number of nodes")

    # Parse the arguments.
    args = parser.parse_args()

    # Set or do something with the resulting arguments.
    VERBOSE = args.verbose
    DEBUG = args.debug
    PCAP = args.pcap
    if(args.nNodes):
        NODES = int(args.nNodes)


def build_scripts():
    """
    Builds the ns3 scripts.
    """
    failure = 0
    print("Building scripts ...")

    try:
        # Set the verbose level.
        stdout_action = subprocess.DEVNULL
        if VERBOSE:
            stdout_action = None

        # Build all the scripts.
        output = subprocess.run([WAF_PATH], shell=True, stdout=stdout_action,
                                cwd=NS3_PATH)

        failure = output.returncode
    except FileNotFoundError:
        print("Error, could not find the 'waf' file in directory " + WAF_PATH +
        "\nPlease run this script in the same directory as the 'waf' file.")
        failure = 1

    if(failure):
        exit()
    else:
        print("Completed building scripts")


def run_scripts(points):
    """
    Run all scripts that are present in the global list 'SCRIPTS'.
    """
    print("Running scripts ...")

    for script in SCRIPTS:
        print("Running " + script + " ...")

        # Run a single script
        s = sim.Sim(WAF_PATH, NS3_PATH, script, verbose=VERBOSE, nNodes=NODES,
                    debug=DEBUG, pcap=PCAP)
        s.run(points)

        print("Finished running " + script)

    print("Completed running scripts")


def main():
    # # Parse the commandline arguments.
    parse_args()

    # # Generate data points.
    p = Points()
    p.generate_points(NODES)
    p.save()
    p.plot()

    # Builds the scripts and checks if we could find the 'waf' file.
    # build_scripts()

    # Run all the scripts.
    # run_scripts(p)


if __name__ == "__main__":
    main()
