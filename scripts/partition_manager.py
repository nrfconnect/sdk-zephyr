#!/usr/bin/env python3
#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic

import argparse
import yaml
import re
from os import path


def remove_item_not_in_list(list_to_remove_from, list_to_check):
    for x in list_to_remove_from:
        if x not in list_to_check and x != 'app':
            list_to_remove_from.remove(x)


def item_is_placed(d, item, after_or_before):
    assert(after_or_before in ['after', 'before'])
    return type(d['placement']) == dict and after_or_before in d['placement'].keys() and \
           d['placement'][after_or_before][0] == item


def remove_irrelevant_requirements(reqs):
    # Remove items dependencies to partitions which are not present
    [[remove_item_not_in_list(reqs[x]['placement'][before_after], reqs.keys())
      for x in reqs.keys() if 'placement' in reqs[x] and type(reqs[x]['placement']) == dict
      and before_after in reqs[x]['placement'].keys()]
     for before_after in ['before', 'after']]


def get_images_which_needs_resolving(reqs):
    return [x for x in reqs.keys() if type(reqs[x]['placement']) == dict and ('before' in reqs[x]['placement'].keys() or
            'after' in reqs[x]['placement'].keys())]


def solve_direction(reqs, unsolved, solution, ab):
    assert(ab in ['after', 'before'])
    current = 'app'
    cont = len(unsolved) > 0
    while cont:
        depends = [x for x in reqs.keys() if item_is_placed(reqs[x], current, ab)]
        if depends:
            assert(len(depends) == 1)
            if ab == 'before':
                solution.insert(solution.index(current), depends[0])
            else:
                solution.insert(solution.index(current) + 1, depends[0])
            current = depends[0]
            unsolved.remove(current)
        else:
            cont = False


def solve_from_last(reqs, unsolved, solution):
    last = [x for x in reqs.keys() if type(reqs[x]['placement']) == str and reqs[x]['placement'] == 'last']
    if last:
        assert(len(last) == 1)
        solution.append(last[0])
        current = last[0]
        cont = True
        while cont:
            depends = [x for x in reqs.keys() if item_is_placed(reqs[x], current, after_or_before='before')]
            if depends:
                solution.insert(solution.index(current), depends[0])
                current = depends[0]
                unsolved.remove(current)
            else:
                cont = False


def extract_sub_partitions(reqs):
    sub_partitions = dict()
    keys_to_delete = list()
    for key, values in reqs.items():
        if 'span' in values.keys():
            sub_partitions[key] = values
            keys_to_delete.append(key)

    for key in keys_to_delete:
        del reqs[key]

    return sub_partitions


def resolve(reqs):
    solution = list(['app'])
    remove_irrelevant_requirements(reqs)
    sub_partitions = extract_sub_partitions(reqs)
    unsolved = get_images_which_needs_resolving(reqs)

    solve_from_last(reqs, unsolved, solution)
    solve_direction(reqs, unsolved, solution, 'before')
    solve_direction(reqs, unsolved, solution, 'after')

    return solution, sub_partitions


def find_partition_size_from_autoconf_h(configs):
    result = dict()
    for config in configs:
        with open(config, 'r') as cf:
            for line in cf.readlines():
                match = re.match(r'#define CONFIG_PM_PARTITION_SIZE_(\w*) (0x[0-9a-fA-F]*)', line)
                if match:
                    if int(match.group(2), 16) != 0:
                        result[match.group(1).lower()] = int(match.group(2), 16)

    return result


def get_list_of_autoconf_files(adr_map):
    return [path.join(props['out_dir'], 'autoconf.h') for props in adr_map.values() if 'out_dir' in props.keys()]


def load_size_config(adr_map):
    configs = get_list_of_autoconf_files(adr_map)
    size_configs = find_partition_size_from_autoconf_h(configs)

    for k, v in adr_map.items():
        if 'size' not in v.keys() and 'span' not in v.keys() and k != 'app':
            adr_map[k]['size'] = size_configs[k]


def load_adr_map(adr_map, input_files):
    for f in input_files:
        img_conf = yaml.safe_load(f)

        adr_map.update(img_conf)
    adr_map['app'] = dict()
    adr_map['app']['placement'] = ''


def set_addresses(reqs, solution, flash_size):
    # First image starts at 0
    reqs[solution[0]]['address'] = 0
    for i in range(1, solution.index('app') + 1):
        current = solution[i]
        previous = solution[i - 1]
        reqs[current]['address'] = reqs[previous]['address'] + reqs[previous]['size']

    has_image_after_app = len(solution) > solution.index('app') + 1
    if has_image_after_app:
        reqs[solution[-1]]['address'] = flash_size - reqs[solution[-1]]['size']
        for i in range(len(solution) - 2, solution.index('app'), -1):
            current = solution[i]
            previous = solution[i + 1]
            reqs[current]['address'] = reqs[previous]['address'] - reqs[current]['size']
        reqs['app']['size'] = reqs[solution[solution.index('app') + 1]]['address'] - reqs['app']['address']
    else:
        reqs['app']['size'] = flash_size - reqs['app']['address']


def set_sub_partition_address_and_size(reqs, sub_partitions):
    first_parent_partition = None
    for sp_name, sp_values in sub_partitions.items():
        size = 0
        for parent_partition in sp_values['span']:
            if parent_partition in reqs:
                if not first_parent_partition:
                    first_parent_partition = parent_partition
                size += reqs[parent_partition]['size']
        if size == 0:
            raise RuntimeError("No compatible parent partition found for %s" % sp_name)
        size = size // len(sp_values['sub_partitions'])
        address = reqs[first_parent_partition]['address']
        for sub_partition in sp_values['sub_partitions']:
            sp_key_name = "%s_%s" % (sp_name, sub_partition)
            reqs[sp_key_name] = dict()
            reqs[sp_key_name]['size'] = size
            reqs[sp_key_name]['address'] = address
            address += size


def get_flash_size(adr_map):
    config = get_list_of_autoconf_files(adr_map)[0]
    with open(config, "r") as cf:
        for line in cf.readlines():
            match = re.match(r'#define CONFIG_FLASH_SIZE (\d*)', line)
            if match:
                return int(match.group(1)) * 1024
        raise RuntimeError("Unable to find 'CONFIG_FLASH_SIZE' in any of: %s" % config)


def get_input_files(input_config):
    input_files = list()
    for k, v in input_config.items():
        input_files.append(open(v['pm.yml'], 'r'))
    return input_files


def add_configurations(adr_map, input_config):
    for k, v in input_config.items():
        adr_map[k]['out_dir'] = v['out_dir']
        adr_map[k]['build_dir'] = v['build_dir']


def get_pm_config(input_config):
    adr_map = dict()
    input_files = get_input_files(input_config)
    load_adr_map(adr_map, input_files)
    add_configurations(adr_map, input_config)
    load_size_config(adr_map)
    flash_size = get_flash_size(adr_map)
    solution, sub_partitions = resolve(adr_map)
    set_addresses(adr_map, solution, flash_size)
    set_sub_partition_address_and_size(adr_map, sub_partitions)
    return adr_map


def get_header_guard_start(filename):
    macro_name = filename.split('.h')[0]
    return '''/* File generated by %s, do not modify */
#ifndef %s_H__
#define %s_H__''' % (__file__, macro_name.upper(), macro_name.upper())


def get_header_guard_end(filename):
    return "#endif /* %s_H__ */" % filename.split('.h')[0].upper()


def get_config_lines(adr_map, head, split):
    lines = list()

    def fn(a, b):
        return lines.append(head + "PM_" + a + split + b)

    for area_name, area_props in sorted(adr_map.items(), key=lambda key_value: key_value[1]['address']):
        fn("%s_ADDRESS" % area_name.upper(), "0x%x" % area_props['address'])
        fn("%s_SIZE" % area_name.upper(), "0x%x" % area_props['size'])
        fn("%s_DEV_NAME" % area_name.upper(), "\"NRF_FLASH_DRV_NAME\"")

    flash_area_id = 0
    for area_name, area_props in sorted(adr_map.items(), key=lambda key_value: key_value[1]['address']):
        fn("%d_LABEL" % flash_area_id, "%s" % area_name.upper())
        adr_map[area_name]['flash_area_id'] = flash_area_id
        flash_area_id += 1

    for area_name, area_props in sorted(adr_map.items(), key=lambda key_value: key_value[1]['flash_area_id']):
        fn("%s_ID" % area_name.upper(), "%d" % area_props['flash_area_id'])
    fn("NUM", "%d" % flash_area_id)

    return lines


def only_sub_image_is_being_built(adr_map, app_output_dir):
    if len(adr_map) == 2:
        non_app_key = [non_app_key for non_app_key in adr_map.keys() if non_app_key != 'app'][0]
        return app_output_dir == adr_map[non_app_key]['out_dir']
    return False


def write_pm_config(adr_map, app_output_dir):
    pm_config_file = "pm_config.h"
    config_lines = get_config_lines(adr_map, "#define ", " ")

    for area_name, area_props in adr_map.items():
        area_props['pm_config'] = list.copy(config_lines)
        area_props['pm_config'].append("#define PM_ADDRESS 0x%x" % area_props['address'])
        area_props['pm_config'].append("#define PM_SIZE 0x%x" % area_props['size'])
        area_props['pm_config'].insert(0, get_header_guard_start(pm_config_file))
        area_props['pm_config'].append(get_header_guard_end(pm_config_file))
        
    # Store complete size/address configuration to all input paths
    for area_name, area_props in adr_map.items():
        if 'out_dir' in area_props.keys():
            write_pm_config_to_file(path.join(area_props['out_dir'], pm_config_file), area_props['pm_config'])

    # Store to root app, but
    if not only_sub_image_is_being_built(adr_map, app_output_dir):
        write_pm_config_to_file(path.join(app_output_dir, pm_config_file), adr_map['app']['pm_config'])


def write_pm_config_to_file(pm_config_file_path, pm_config):
    with open(pm_config_file_path, 'w') as out_file:
        out_file.write('\n'.join(pm_config))


def write_kconfig_file(adr_map, app_output_dir):
    pm_kconfig_file = "pm.config"
    config_lines = get_config_lines(adr_map, "", "=")

    # Store out dir and build dir
    for area_name, area_props in adr_map.items():
        if 'out_dir' in area_props.keys():
            config_lines.append('PM_%s_OUT_DIR="%s"' % (area_name.upper(), area_props['out_dir']))
            config_lines.append('PM_%s_BUILD_DIR="%s"' % (area_name.upper(), area_props['build_dir']))

    # Store output dir to app
    config_lines.append('PM_APP_OUT_DIR "%s"' % app_output_dir)

    # Store complete size/address configuration to all input paths
    for area_name, area_props in adr_map.items():
        if 'out_dir' in area_props.keys():
            write_pm_config_to_file(path.join(area_props['out_dir'], pm_kconfig_file), config_lines)

    # Store to root app
    if not only_sub_image_is_being_built(adr_map, app_output_dir):
        write_pm_config_to_file(path.join(app_output_dir, pm_kconfig_file), config_lines)


def parse_args():
    parser = argparse.ArgumentParser(
        description='''Parse given 'pm.yml' partition manager configuration files to deduce the placement of partitions.

The partitions and their relative placement is defined in the 'pm.yml' files. The path to the 'pm.yml' files are used
to locate 'autoconf.h' files. These are used to find the partition sizes, as well as the total flash size.

This script generates a file for each partition - "pm_config.h".
This file contains all addresses and sizes of all partitions.

"pm_config.h" is in the same folder as the given 'pm.yml' file.''',
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-i", "--input", required=True, type=str, nargs="+",
                        help="Space separated list of configs."
                             "Each config is a ':' - separated list of the following properties:"
                             "image-name:pm.yml-path:build_dir:out_dir")
    parser.add_argument("--app-pm-config-dir", required=True,
                        help="Where to store the 'pm_config.h' of the root app.")

    args = parser.parse_args()

    return args


def main():
    args = parse_args()
    if args.input is not None:
        input_config = dict()
        for i in args.input:
            split = i.split(':')
            input_config[split[0]] = dict()
            input_config[split[0]]['pm.yml'] = split[1]
            input_config[split[0]]['build_dir'] = split[2]
            input_config[split[0]]['out_dir'] = split[3]
        pm_config = get_pm_config(input_config)
        write_pm_config(pm_config, args.app_pm_config_dir)
        write_kconfig_file(pm_config, args.app_pm_config_dir)
    else:
        print("No input, running tests.")
        test()


def test():
    td = {'spm': {'placement': {'before': ['app']}, 'size': 100},
          'mcuboot': {'placement': {'before': ['spm', 'app']}, 'size': 200},
          'mcuboot_partitions': {'span': ['spm', 'app'], 'sub_partitions': ['primary', 'secondary']},
          'app': {'placement': ''}}
    s, sub_partitions = resolve(td)
    set_addresses(td, s, 1000)
    set_sub_partition_address_and_size(td, sub_partitions)

    td = {'mcuboot': {'placement': {'before': ['app']}, 'size': 200},
          'mcuboot_partitions': {'span': ['spm', 'app'], 'sub_partitions': ['primary', 'secondary']},
          'app': {'placement': ''}}
    s, sub_partitions = resolve(td)
    set_addresses(td, s, 1000)
    set_sub_partition_address_and_size(td, sub_partitions)

    td = {
        'e': {'placement': {'before': ['app']}, 'size': 100},
        'a': {'placement': {'before': ['b']}, 'size': 100},
        'd': {'placement': {'before': ['e']}, 'size': 100},
        'c': {'placement': {'before': ['d']}, 'size': 100},
        'j': {'placement': 'last', 'size': 20},
        'i': {'placement': {'before': ['j']}, 'size': 20},
        'h': {'placement': {'before': ['i']}, 'size': 20},
        'f': {'placement': {'after': ['app']}, 'size': 20},
        'g': {'placement': {'after': ['f']}, 'size': 20},
        'b': {'placement': {'before': ['c']}, 'size': 20},
        'app': {'placement': ''}}
    s, _ = resolve(td)
    set_addresses(td, s, 1000)

    td = {'mcuboot': {'placement': {'before': ['app', 'spu']}, 'size': 200},
          'b0': {'placement': {'before': ['mcuboot', 'app']}, 'size': 100},
          'app': {'placement': ''}}
    s, _ = resolve(td)
    set_addresses(td, s, 1000)

    td = {'b0': {'placement': {'before': ['mcuboot', 'app']}, 'size': 100}, 'app': {'placement': ''}}
    s, _ = resolve(td)
    set_addresses(td, s, 1000)

    td = {'spu': {'placement': {'before': ['app']}, 'size': 100},
          'mcuboot': {'placement': {'before': ['spu', 'app']}, 'size': 200},
          'app': {'placement': ''}}
    s, _ = resolve(td)
    set_addresses(td, s, 1000)

    td = {'provision': {'placement': 'last', 'size': 100},
          'mcuboot': {'placement': {'before': ['spu', 'app']}, 'size': 100},
          'b0': {'placement': {'before': ['mcuboot', 'app']}, 'size': 50},
          'spu': {'placement': {'before': ['app']}, 'size': 100},
          'app': {'placement': ''}}
    s, _ = resolve(td)
    set_addresses(td, s, 1000)
    pass


if __name__ == "__main__":
    main()
