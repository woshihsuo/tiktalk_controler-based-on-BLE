#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.

import json
import os
import sys
import time
import shutil

from utils.build_utils import root_path, script_path, target_config_path, output_root
from utils.build_utils import pkg_tools_path, jlink_tools_path, lzma_tools_path, sign_tools_path, derived_tools_path
from utils.build_utils import CopyModule, exec_shell, cmp_file, rm_pyc, rm_all, fn_get_subdirs
from enviroment import TargetEnvironment
from sdk_generator.target_config_genarator import genarate_reserve_config

sdk_copy_common_files = [
    os.path.join(script_path),
    os.path.join(root_path, 'build.py'),
    os.path.join(target_config_path),
    os.path.join(pkg_tools_path),
    os.path.join(jlink_tools_path),
    os.path.join(lzma_tools_path),
    os.path.join(sign_tools_path),
    os.path.join(derived_tools_path),
]

sdk_close_components = [
]

# sdk构建的拦截白名单，名单内的芯片，sdk构建必须指定开源管理规则：open_source_path
sdk_intercept_list = [
    'bs21a', 'bs20', 'bs20h', 'bs21', 'bs21e', 'bs22', 'bs26'
]

def compare_path_bin(path1, path2):
    for file in os.listdir(path1):
        if not file.endswith('.bin'):
            continue
        check_exclude = ('signed.bin', 'sign.bin', 'sign_a.bin', 'sign_b.bin')
        if file.endswith(check_exclude):
            continue
        f1 = os.path.join(path1, file)
        f2 = os.path.join(path2, file)
        print("Comparing:")
        print(f1)
        print(f2)
        if not cmp_file(f1, f2):
            print("DIFF")
            return False
        print("SAME")
    return True


class SdkGenerator:
    def __init__(self, env: TargetEnvironment, sdk_root_path: str):
        self.env = env
        self.sdk_root_path = sdk_root_path
        replace_suffix = ['.srcrelease']
        if self.env.get('replace_suffix', False):
            replace_suffix = self.env.get('replace_suffix', False)
        print(replace_suffix)
        self.copy_module = CopyModule(replace_root=sdk_root_path, replace_suffix=replace_suffix, copy_header = False)
        self.org_target_output_path = []
        self.rm_line_map = {}
        self.sub_cmake = {}
        self.all_cmake = {}
        chip = self.env.get("chip", False)
        if not isinstance(chip, list):
            self.chip = [chip]
        else:
            self.chip = chip
        self.sdk_copy_module_mask_add()
        self.copy_module.append_mask('.srcrelease')

    def parse_depend_cmake_files(self, cmake_trace_file):
        with open(cmake_trace_file, 'r') as fp:
            lines = fp.readlines()

        sdk_cmake_dict = {}
        depend_cmake_files = []

        for line in lines:
            dict_ = json.loads(line)
            if 'file' not in dict_:
                continue
            f = dict_['file']
            if not f.startswith(root_path) or f.startswith(output_root):
                continue
            if 'cmd' in dict_ and dict_['cmd'] == 'add_subdirectory':
                if f not in self.sub_cmake:
                    self.sub_cmake[f] = set()
                file_path = os.path.dirname(f)
                if "$" in dict_['args'][0]:
                    continue
                depend_cmake = os.path.join(file_path, dict_['args'][0], "CMakeLists.txt")
                self.sub_cmake[f].add((int(dict_['line']), depend_cmake))
            if f in sdk_cmake_dict:
                continue
            sdk_cmake_dict[f] = None
            self.all_cmake[f] = None
            tmp = f.replace(root_path,  self.sdk_root_path)
            if self.is_close_component(tmp) == False:
                if tmp.endswith("CMakeLists.txt"):
                    if os.path.exists(os.path.dirname(tmp)):
                        depend_cmake_files.append(f)
                else:
                    depend_cmake_files.append(f)
        return depend_cmake_files

    def copy_menuconfig(self):
        if self.env.get('reload_kconfig'):
            # reset sdk menuconfig and copy sdk depend menuconfig.
            sdk_target = self.env.get('pkg_target_name', False)
            if not isinstance(sdk_target, list):
                sdk_target = [sdk_target]
            kconfig_cp_list = []
            for target in sdk_target:
                target_env = TargetEnvironment(target)
                config_path = os.path.join(self.sdk_root_path, 'build', 'config', 'target_config',
                                           target_env.get('chip'), 'menuconfig', target_env.get('core'))
                if os.path.exists(config_path):
                    shutil.rmtree(config_path)
                kconfig_cp_list.append(os.path.join(root_path, 'build', 'config', 'target_config', target_env.get('chip'),
                                       'menuconfig', target_env.get('core'), target.replace('-', '_') + '.config'))
            self.copy_srcs(kconfig_cp_list)
        else:
            for chip in self.chip:
                if os.path.exists(os.path.join(root_path, 'build', 'config', 'target_config', chip, 'menuconfig')):
                    self.copy_srcs([os.path.join(root_path, 'build', 'config', 'target_config', chip, 'menuconfig')])
        self.copy_srcs([os.path.join(root_path, 'config.in')])
        self.copy_srcs([os.path.join(root_path, 'build', 'config', 'target_config', 'test', 'menuconfig', 'samples')])

    def copy_kconfig(self, cmake_trace_file):
        depend_cmake_files = self.parse_depend_cmake_files(cmake_trace_file)
        kconfig_fils = []
        for f in depend_cmake_files:
            tmp = f.replace("CMakeLists.txt", 'Kconfig')
            if os.path.exists(tmp):
                kconfig_fils.append(tmp)
        self.copy_srcs(kconfig_fils)

    def copy_depends(self, cmake_trace_file):
        depend_cmake_files = self.parse_depend_cmake_files(cmake_trace_file)
        cmake_dest = self.copy_srcs(depend_cmake_files)
        for path in sdk_copy_common_files:
            if not os.path.exists(path):
                sdk_copy_common_files.remove(path)
        self.copy_srcs(sdk_copy_common_files)
        if self.env.get('use_memuconfig') != False:
            self.copy_menuconfig()
            self.copy_kconfig(cmake_trace_file)
        if self.env.get('auto_gen_config'):
            self.genarate_sdk_target_config(self.env.get('pkg_target_name', False))

    def genarate_sdk_target_config(self, targets):
        reserve = {}
        for target in targets:
            env = TargetEnvironment(target)
            chip = env.get('chip')
            if chip not in reserve:
                reserve[chip] = {'target': [], 'template': []}
            if target not in reserve[chip]['target']:
                reserve[chip]['target'].append(target)
            if env.get_target_template() not in reserve[chip]['template']:
                reserve[chip]['template'].append(env.get_target_template())

        print(reserve)
        for chip in reserve:
            path = os.path.join(self.sdk_root_path, 'build', 'config', 'target_config', chip)
            config_path = os.path.join(path, 'config.py')
            template_path = os.path.join(path, 'target_config.py')
            genarate_reserve_config(reserve[chip]['target'], config_path)
            genarate_reserve_config(reserve[chip]['template'], template_path)


    def rm_lines_in_file(self, _file, lines):
        with open(_file, 'r') as fp_read:
            text = fp_read.readlines()
            lines = sorted(list(set(lines)), reverse=True)
            for idx in lines:
                text.pop(idx - 1)

        with open(_file, 'w') as fp_write:
            fp_write.write("".join(text))

    def copy_srcs(self, file_list):
        dest_src = []
        for file in file_list:
            if not os.path.exists(file):
                print("SDK GENERATE ERROR!!")
                print("FILE: %s is not exists!!" % file)
                raise
            dest = self.copy_module.copy(file)
            if dest is not None:
                dest_src.append(dest)
        return dest_src

    def copy_headers(self, file_list):
        dest_headers = []
        for file in file_list:
            if not os.path.exists(file):
                print("SDK GENERATE ERROR!!")
                print("FILE: %s is not exists!!" % file)
                raise
            if os.path.isfile(file) and file.endswith('.h'):
                # 文件且以.h结尾时直接拷贝
                dest = self.copy_module.copy_file(file)
                if dest is not None:
                    dest_headers.append(dest)
            elif os.path.isdir(file):
                # 目录时遍历并拷贝头文件
                for root, dirs, files in os.walk(file):
                    for f in files:
                        if f.endswith('.h'):
                            src_file = os.path.join(root, f)
                            dest = self.copy_module.copy_file(src_file)
                            if dest is not None:
                                dest_headers.append(dest)
                continue
            else:
                # 其他情况跳过
                continue
        return dest_headers

    def register_org_target_path(self, path):
        self.org_target_output_path.append(path)

    def rm_cmake_lines(self):
        rm_lines = {}
        for f, line_dep in self.sub_cmake.items():
            for line, dep_cmake in line_dep:
                if dep_cmake in self.all_cmake:
                    continue
                if f not in rm_lines:
                    rm_lines[f] = []
                rm_lines[f].append(line)
        for f, lines in rm_lines.items():
            sdk_file = f.replace(root_path, self.sdk_root_path)
            self.rm_lines_in_file(sdk_file, lines)

    def sdk_copy_module_mask_add(self):
        chip_mask = []
        path_list = [target_config_path]
        for path_mask in path_list:
            for name in fn_get_subdirs(path_mask):
                if name not in self.chip and name not in chip_mask:
                    chip_mask.append(name)
        self.copy_module.append_mask(chip_mask)

    def is_closed_component(self, component_name):
        closed_components = self.env.get('closed_components', cmake_type=False)
        open_components = self.env.get('open_components', cmake_type=False)
        if isinstance(closed_components, list) and component_name in closed_components:
            return True
        elif isinstance(open_components, list) and component_name not in open_components:
            return True
        return False

    def sdk_delete_tmp_files(self):
        delete_files = []
        delete_files.append(os.path.join(output_root, 'sdk', 'output'))
        delete_files.append(os.path.join(output_root, 'sdk', 'make.cmd'))
        for dir_path, dir_names, file_names in os.walk(os.path.join(output_root, 'sdk', 'interim_binary', self.env.get('chip'), 'libs'), topdown=False):
            for name in file_names:
                if name.endswith('.a'):
                    if not self.is_closed_component(name[3:-2]):
                        delete_files.append(os.path.join(dir_path, name))
        for dir_path, dir_names, file_names in os.walk(os.path.join(output_root, 'sdk', 'build', 'config', 'target_config', self.env.get('chip')), topdown=False):
            for name in file_names:
                if name.endswith('.srcrelease'):
                    delete_files.append(os.path.join(dir_path, name))
        rm_all(delete_files)
        rm_pyc(os.path.join(output_root, 'sdk', 'build'))
        rm_pyc(os.path.join(output_root, 'sdk', 'tools', 'pkg'))

    def sdk_build(self, build_time, nhso, build_level):
        org_pwd = os.getcwd()
        os.chdir(self.sdk_root_path)
        build_targets = self.env.get('pkg_target_name', cmake_type=False)
        print(build_targets)
        sdk_type_list = self.env.get('sdk_type').split(';')
        for idx, target in enumerate(build_targets):
            sdk_build_cmd = ['./build.py', target]
            if build_time != '':
                sdk_build_cmd.append("-build_time=%s" %build_time)
            if nhso == True:
                sdk_build_cmd.append("-nhso")
            if build_level == 'release':
                sdk_build_cmd.append("-release")

            ret_code = exec_shell(sdk_build_cmd)
            if ret_code:
                sys.exit(1)
            org_output_path = self.org_target_output_path[idx]
            sdk_output_path = org_output_path.replace(root_path, self.sdk_root_path)
            if not compare_path_bin(sdk_output_path, org_output_path):
                print("sdk build failed")
                sys.exit(1)
        os.chdir(org_pwd)
        if "makefile" in sdk_type_list:
            self.sdk_makefile_build(nhso, build_level)
        self.sdk_delete_tmp_files()

    # sdk 生成后，二次编译验证前执行
    def pre_sdk_build_check(self, chip, sdk_output_path, sdk_pkg_target_name):
        sdk_pre_sdk_py = os.path.join(target_config_path, chip, 'scripts', 'pre_sdk_build.py')
        if os.path.exists(sdk_pre_sdk_py):
            print("pre process sdk before build...")
            args = [sys.executable, sdk_pre_sdk_py, sdk_output_path]
            for target in sdk_pkg_target_name:
                args.append(target)
            exec_shell(args)

        if not all(chip_item in sdk_intercept_list for chip_item in self.chip):
            # 不在白名单的芯片不检查
            return

        open_source_path = self.env.get('open_source_path')
        if open_source_path:
            sdk_source_release_path = os.path.join(root_path, open_source_path)
            if os.path.exists(sdk_source_release_path):
                with open(sdk_source_release_path, 'r', encoding='utf-8') as file:
                    source_release = json.load(file)
                    # 拷贝指定文件到sdk目录
                    cp_src = [os.path.join(root_path, path) for path in source_release["sdk_copy"]["path"]]
                    self.copy_srcs(cp_src)
                    # 拷贝头文件（.h）到sdk目录
                    cp_headers = [os.path.join(root_path, path) for path in source_release["closed_components"]["header"]]
                    self.copy_headers(cp_headers)

                    # 删除sdk_delete指定的文件
                    self.delete_sub_paths(sdk_output_path, source_release["sdk_delete"]["path"])

                    # sdk文件扫描
                    self.check_sdk_files(sdk_output_path, source_release)

    def is_close_component(self, path):
        for f in sdk_close_components:
            tmp = f.replace(root_path,  self.sdk_root_path)
            if path.startswith(tmp):
                return True
        return False

    def check_sdk_files(self, sdk_path, sdk_config):
        """
        检查SDK拦截路径中的文件是否在放行列表中

        Args:
            sdk_config (dict): SDK配置信息, 包含intercept_path, intercept_type, open_files等
        """

        # 获取配置信息
        intercept_paths = sdk_config["sdk_intercept"]["intercept_path"]
        intercept_types = sdk_config["sdk_intercept"]["intercept_type"]
        open_files = [os.path.join(sdk_path, file_path) for file_path in sdk_config["sdk_intercept"]["open_files"]]

        # 存储未放行的文件
        unallowed_files = []
        # 遍历所有拦截路径
        for path in intercept_paths:
            check_path = os.path.join(sdk_path, path)
            # 检查路径是否存在
            if not os.path.exists(check_path):
                print(f"Warning: sdk拦截路径不存在 - {check_path}")
                continue

            # 遍历路径下的所有文件
            for root, dirs, files in os.walk(check_path):
                for file in files:
                    # 检查文件扩展名是否匹配拦截类型
                    if any(file.endswith(ext) for ext in intercept_types):
                        # 构造完整文件路径
                        full_path = os.path.join(root, file)

                        # 检查文件是否在放行列表中
                        if full_path not in open_files:
                            unallowed_files.append(full_path)

        # 如果存在未放行的文件，打印并报错
        if unallowed_files:
            print("Error: 发现未放行的拦截文件")
            for file in unallowed_files:
                print(f"  {file}")
            exit(-1)

    def sdk_makefile_build(self, nhso, build_level):
        with open(os.path.join(self.sdk_root_path, 'make.cmd'), 'r') as fp:
            lines = fp.readlines()
        idx = 0
        for line in lines:
            org_pwd = os.getcwd()
            start_time = time.time()
            line = line.replace('\n', '')
            os.chdir(line)
            sdk_build_cmd = ['make', '-j48']
            if nhso == True:
                sdk_build_cmd.append("nhso=true")
            if build_level == 'release':
                sdk_build_cmd.append("build_level=release")
            ret_code = exec_shell(sdk_build_cmd)
            if ret_code:
                sys.exit(1)
            end_time = time.time()
            print("build %s takes %f s" %  (line, end_time - start_time))
            org_output_path = self.org_target_output_path[idx]
            sdk_output_path_makefile = "%s-makefile" % org_output_path.replace(root_path, self.sdk_root_path)
            if not compare_path_bin(sdk_output_path_makefile, org_output_path):
                print("sdk build failed")
                sys.exit(1)
            os.chdir(org_pwd)
            idx = idx + 1

    def delete_sub_paths(self, base_path, sub_paths):
        for sub in sub_paths:
            full = os.path.join(base_path, sub)
            if os.path.exists(full):
                print(f"Rm SDK: {full}")
                if os.path.isfile(full):
                    os.remove(full)
                elif os.path.isdir(full):
                    try:
                        shutil.rmtree(full)
                    except Exception as e:
                        print(f"Failed rm SDK: {full}, Error: {e}")
            else:
                print(f"Rm path not found, skipped: {full}")

    def check_sdk_components(self, sdk_target_env):
        """
        检查ram_component中的组件是否都在其他列表中

        Args:
            sdk_target_env (dict): SDK目标target
            source_release (dict): 源发布配置

        Returns:
            bool: 检查是否通过
        """
        if not all(chip_item in sdk_intercept_list for chip_item in self.chip):
            # 不在白名单的芯片不检查
            return

        open_source_path = self.env.get('open_source_path')
        if open_source_path:
            sdk_source_release_path = os.path.join(root_path, open_source_path)
            if os.path.exists(sdk_source_release_path):
                source_release = {}
                with open(sdk_source_release_path, 'r', encoding='utf-8') as file:
                    source_release = json.load(file)
                # 获取合并后的ram_component内容
                ram_components_str = sdk_target_env.get('ram_component')

                if not ram_components_str:
                    return False

                ram_components = [comp.strip() for comp in ram_components_str.split(';') if comp.strip()]

                # 获取所有开放组件列表
                open_components = source_release["open_components"]["components"]
                closed_components = source_release["closed_components"]["components"]

                # 转换为集合并求交集
                open_set = set(open_components)
                closed_set = set(closed_components)
                intersection = open_set & closed_set

                # 检查是否存在交集
                if intersection:
                    # 存在交集，报错并打印交集内容
                    print("Error: 开闭源组件存在重复定义")
                    for comp in intersection:
                        print(f"\"{comp}\",")
                    exit(-1)

                sdk_target_env.append("closed_components", ";".join(str(component) for component in closed_components))
                self.env.append("closed_components", ";".join(str(component) for component in closed_components))

                # 合并所有组件列表
                all_components = set(open_components + closed_components)

                # 检查每个ram组件是否存在于所有组件中
                missing_components = []
                for component in ram_components:
                    if component not in all_components:
                        missing_components.append(component)

                # 如果有缺失的组件，打印错误并退出
                if missing_components:
                    print(f"Error: {sdk_target_env.config['target_command']} 以下组件未在开源发布文件指定")
                    print(f"SDK开源发布文件:{sdk_source_release_path}")
                    for comp in missing_components:
                        print(f"\"{comp}\",")
                    exit(-1)
                return True
            else:
                print("Warning: open_source_path路径不存在")
                exit(-1)
        else:
            print("Warning: 请指定sdk构建使用的open_source_path路径")
            exit(-1)