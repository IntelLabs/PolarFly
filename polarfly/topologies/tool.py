# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT License

# Author: Alessandro Maissen, Kartik Lakhotia

import topogen as tg
from topogen import validate_brown as vbr
from topogen import validate_brown_ext as vbe
from topogen import validate_polarstar as vps


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(prog='tool.py')
    subparser = parser.add_subparsers(help="type of operation", dest='type' ,required=True)

    # Topology Generator 
    parser_generate = subparser.add_parser("generate", help="generates a topology") 
    parser_generate_subparser = parser_generate.add_subparsers(help='type of topology', dest='type', required=True)

    topology_generator_parsers = []


    # Brown
    parser_generate_brown = parser_generate_subparser.add_parser('brown', help='generates a Brown topology')
    parser_generate_brown.add_argument('q', type=int, help='specifies the size of Gallois Field (q: size of Galois Field, is a prime power')
    parser_generate_brown.set_defaults(func=tg.BrownGenerator().generate)
    topology_generator_parsers.append(parser_generate_brown)

    # Brown Extensions
    parser_generate_brown_ext = parser_generate_subparser.add_parser('brown_ext', help='generates incrmeental expansions of a Brown topology')
    parser_generate_brown_ext.add_argument('q', type=int, help='specifies the size of Gallois Field (q: size of Galois Field, is a prime power)')
    parser_generate_brown_ext.add_argument('r0', type=int, help='number of replications of cluster C0')
    parser_generate_brown_ext.add_argument('r1', type=int, help='round robin replication of a selected quadric and its neighbors(if r1>0, r0 is ignored)')
    parser_generate_brown_ext.set_defaults(func=tg.BrownExtGenerator().generate)
    topology_generator_parsers.append(parser_generate_brown_ext)

    # Polarstar
    parser_generate_polarstar = parser_generate_subparser.add_parser('polarstar', help='generate Polarstar topology')   
    parser_generate_polarstar.add_argument('d', type=int, help='network radix, must be >= 2')
    parser_generate_polarstar.add_argument('sg', type=str, choices=['iq', 'paley', 'max'], default="max", help='supernode graph : iq, paley or max')
    parser_generate_polarstar.set_defaults(func=tg.PolarstarGenerator().generate)
    topology_generator_parsers.append(parser_generate_polarstar) 

    for sub in topology_generator_parsers:
        sub.add_argument('-v','--validate', action='store_true', help='validates the generated topology')
        sub.set_defaults(save=True)

    # end Topology Generator

    # Topology Validator
    parser_validate = subparser.add_parser("validate", help="validates a topology")
    parser_validate_subparser = parser_validate.add_subparsers(help='type of topology', dest='type', required=True)

    # Brown
    parser_validate_brown = parser_validate_subparser.add_parser("brown", help='validates Brown topologies')
    parser_validate_brown.set_defaults(func=vbr.validate_brown)

    # Brown Extensions
    parser_validate_brown_ext = parser_validate_subparser.add_parser("brown_ext", help='validates expanded Brown topologies')
    parser_validate_brown_ext.set_defaults(func=vbe.validate_brown_ext)

    # Polarstar
    parser_validate_polarstar = parser_validate_subparser.add_parser("polarstar", help='validates polarstar topologies')
    parser_validate_polarstar.set_defaults(func=vps.validate_polarstar)

    # end Topology Validator

    # Topo information getter
    parser_info = subparser.add_parser("info", help="saves information about selected topologies")
    parser_info.add_argument('-t', '--topos', type=str, nargs='+', choices=[topo for topo in tg.toponames.keys() if topo != 'JF'], required=True, help="specifies the topologies")
    parser_info.add_argument('-c', '--classes', type=int, nargs='+', required=True, help="specifies the classes defining the number of host a topology have")


    # Parsing arguments and invoke function
    kwargs = parser.parse_args()
    func = kwargs.func
    del kwargs.func
    del kwargs.type # because dest
    func(**kwargs.__dict__)
