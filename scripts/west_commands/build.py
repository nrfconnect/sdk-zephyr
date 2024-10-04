# Copyright (c) 2018 Foundries.io
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import pathlib
import shlex
import sys
import yaml

from west import log
from west.configuration import config
from zcmake import DEFAULT_CMAKE_GENERATOR, run_cmake, run_build, CMakeCache
from build_helpers import is_zephyr_build, find_build_dir, load_domains, \
    FIND_BUILD_DIR_DESCRIPTION

from zephyr_ext_common import Forceable

_ARG_SEPARATOR = '--'

SYSBUILD_PROJ_DIR = pathlib.Path(__file__).resolve().parent.parent.parent \
                    / pathlib.Path('share/sysbuild')

BUILD_USAGE = '''\
west build [-h] [-b BOARD[@REV]]] [-d BUILD_DIR]
           [-S SNIPPET] [--shield SHIELD]
           [-t TARGET] [-p {auto, always, never}] [-c] [--cmake-only]
           [-n] [-o BUILD_OPT] [-f]
           [--sysbuild | --no-sysbuild] [--domain DOMAIN]
           [source_dir] -- [cmake_opt [cmake_opt ...]]
'''

BUILD_DESCRIPTION = f'''\
Convenience wrapper for building Zephyr applications.

{FIND_BUILD_DIR_DESCRIPTION}

positional arguments:
  source_dir            application source directory
  cmake_opt             extra options to pass to cmake; implies -c
                        (these must come after "--" as shown above)
'''

PRISTINE_DESCRIPTION = """\
A "pristine" build directory is empty. The -p option controls
whether the build directory is made pristine before the build
is done. A bare '--pristine' with no value is the same as
--pristine=always. Setting --pristine=auto uses heuristics to
guess if a pristine build may be necessary."""

def _banner(msg):
    log.inf('-- west build: ' + msg, colorize=True)

def config_get(option, fallback):
    return config.get('build', option, fallback=fallback)

def config_getboolean(option, fallback):
    return config.getboolean('build', option, fallback=fallback)

class AlwaysIfMissing(argparse.Action):

    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, values or 'always')

class Build(Forceable):

    def __init__(self):
        super(Build, self).__init__(
            'build',
            # Keep this in sync with the string in west-commands.yml.
            'compile a Zephyr application',
            BUILD_DESCRIPTION,
            accepts_unknown_args=True)

        self.source_dir = None
        '''Source directory for the build, or None on error.'''

        self.build_dir = None
        '''Final build directory used to run the build, or None on error.'''

        self.created_build_dir = False
        '''True if the build directory was created; False otherwise.'''

        self.run_cmake = False
        '''True if CMake was run; False otherwise.

        Note: this only describes CMake runs done by this command. The
        build system generated by CMake may also update itself due to
        internal logic.'''

        self.cmake_cache = None
        '''Final parsed CMake cache for the build, or None on error.'''

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description,
            usage=BUILD_USAGE)

        # Remember to update west-completion.bash if you add or remove
        # flags

        parser.add_argument('-b', '--board',
                        help='board to build for with optional board revision')
        # Hidden option for backwards compatibility
        parser.add_argument('-s', '--source-dir', help=argparse.SUPPRESS)
        parser.add_argument('-d', '--build-dir',
                            help='build directory to create or use')
        self.add_force_arg(parser)

        group = parser.add_argument_group('cmake and build tool')
        group.add_argument('-c', '--cmake', action='store_true',
                           help='force a cmake run')
        group.add_argument('--cmake-only', action='store_true',
                           help="just run cmake; don't build (implies -c)")
        group.add_argument('--domain', action='append',
                           help='''execute build tool (make or ninja) only for
                           given domain''')
        group.add_argument('-t', '--target',
                           help='''run build system target TARGET
                           (try "-t usage")''')
        group.add_argument('-T', '--test-item',
                           help='''Build based on test data in testcase.yaml
                           or sample.yaml. If source directory is not used
                           an argument has to be defined as
                           SOURCE_PATH/TEST_NAME.
                           E.g. samples/hello_world/sample.basic.helloworld.
                           If source directory is passed
                           then "TEST_NAME" is enough.''')
        group.add_argument('-o', '--build-opt', default=[], action='append',
                           help='''options to pass to the build tool
                           (make or ninja); may be given more than once''')
        group.add_argument('-n', '--just-print', '--dry-run', '--recon',
                            dest='dry_run', action='store_true',
                            help="just print build commands; don't run them")
        group.add_argument('-S', '--snippet', dest='snippets', metavar='SNIPPET',
                           action='append', default=[],
                           help='''add the argument to SNIPPET; may be given
                           multiple times. Forces CMake to run again if given.
                           Do not use this option with manually specified
                           -DSNIPPET... cmake arguments: the results are
                           undefined''')
        group.add_argument('--shield', dest='shields', metavar='SHIELD',
                           action='append', default=[],
                           help='''add the argument to SHIELD; may be given
                           multiple times. Forces CMake to run again if given.
                           Do not use this option with manually specified
                           -DSHIELD... cmake arguments: the results are
                           undefined''')

        group = parser.add_mutually_exclusive_group()
        group.add_argument('--sysbuild', action='store_true',
                           help='''create multi domain build system''')
        group.add_argument('--no-sysbuild', action='store_true',
                           help='''do not create multi domain build system
                                   (default)''')

        group = parser.add_argument_group('pristine builds',
                                          PRISTINE_DESCRIPTION)
        group.add_argument('-p', '--pristine', choices=['auto', 'always',
                            'never'], action=AlwaysIfMissing, nargs='?',
                            help='pristine build folder setting')

        return parser

    def do_run(self, args, remainder):
        self.args = args        # Avoid having to pass them around
        self.config_board = config_get('board', None)
        log.dbg('args: {} remainder: {}'.format(args, remainder),
                level=log.VERBOSE_EXTREME)
        # Store legacy -s option locally
        source_dir = self.args.source_dir
        self._parse_remainder(remainder)
        # Parse testcase.yaml or sample.yaml files for additional options.
        if self.args.test_item:
            # we get path + testitem
            item = os.path.basename(self.args.test_item)
            if self.args.source_dir:
                test_path = self.args.source_dir
            else:
                test_path = os.path.dirname(self.args.test_item)
            if test_path and os.path.exists(test_path):
                self.args.source_dir = test_path
                if not self._parse_test_item(item):
                    log.die("No test metadata found")
            else:
                log.die("test item path does not exist")

        if source_dir:
            if self.args.source_dir:
                log.die("source directory specified twice:({} and {})".format(
                                            source_dir, self.args.source_dir))
            self.args.source_dir = source_dir
        log.dbg('source_dir: {} cmake_opts: {}'.format(self.args.source_dir,
                                                       self.args.cmake_opts),
                level=log.VERBOSE_EXTREME)
        self._sanity_precheck()
        self._setup_build_dir()

        if args.pristine is not None:
            pristine = args.pristine
        else:
            # Load the pristine={auto, always, never} configuration value
            pristine = config_get('pristine', 'never')
            if pristine not in ['auto', 'always', 'never']:
                log.wrn(
                    'treating unknown build.pristine value "{}" as "never"'.
                    format(pristine))
                pristine = 'never'
        self.auto_pristine = pristine == 'auto'

        log.dbg('pristine: {} auto_pristine: {}'.format(pristine,
                                                        self.auto_pristine),
                level=log.VERBOSE_VERY)
        if is_zephyr_build(self.build_dir):
            if pristine == 'always':
                self._run_pristine()
                self.run_cmake = True
            else:
                self._update_cache()
                if (self.args.cmake or self.args.cmake_opts or
                        self.args.cmake_only or self.args.snippets or
                        self.args.shields):
                    self.run_cmake = True
        else:
            self.run_cmake = True
        self.source_dir = self._find_source_dir()
        self._sanity_check()

        board, origin = self._find_board()
        self._run_cmake(board, origin, self.args.cmake_opts)
        if args.cmake_only:
            return

        self._sanity_check()
        self._update_cache()
        self.domains = load_domains(self.build_dir)

        self._run_build(args.target, args.domain)

    def _find_board(self):
        board, origin = None, None
        if self.cmake_cache:
            board, origin = (self.cmake_cache.get('CACHED_BOARD'),
                             'CMakeCache.txt')

            # A malformed CMake cache may exist, but not have a board.
            # This happens if there's a build error from a previous run.
            if board is not None:
                return (board, origin)

        if self.args.board:
            board, origin = self.args.board, 'command line'
        elif 'BOARD' in os.environ:
            board, origin = os.environ['BOARD'], 'env'
        elif self.config_board is not None:
            board, origin = self.config_board, 'configfile'
        return board, origin

    def _parse_remainder(self, remainder):
        self.args.source_dir = None
        self.args.cmake_opts = None

        try:
            # Only one source_dir is allowed, as the first positional arg
            if remainder[0] != _ARG_SEPARATOR:
                self.args.source_dir = remainder[0]
                remainder = remainder[1:]
            # Only the first argument separator is consumed, the rest are
            # passed on to CMake
            if remainder[0] == _ARG_SEPARATOR:
                remainder = remainder[1:]
            if remainder:
                self.args.cmake_opts = remainder
        except IndexError:
            pass

    def _parse_test_item(self, test_item):
        found_test_metadata = False
        for yp in ['sample.yaml', 'testcase.yaml']:
            yf = os.path.join(self.args.source_dir, yp)
            if not os.path.exists(yf):
                continue
            found_test_metadata = True
            with open(yf, 'r') as stream:
                try:
                    y = yaml.safe_load(stream)
                except yaml.YAMLError as exc:
                    log.die(exc)
            common = y.get('common')
            tests = y.get('tests')
            if not tests:
                log.die(f"No tests found in {yf}")
            if test_item not in tests:
                log.die(f"Test item {test_item} not found in {yf}")
            item = tests.get(test_item)

            sysbuild = False
            extra_dtc_overlay_files = []
            extra_overlay_confs = []
            extra_conf_files = []
            required_snippets = []
            for section in [common, item]:
                if not section:
                    continue
                sysbuild = section.get('sysbuild', sysbuild)
                for data in [
                        'extra_args',
                        'extra_configs',
                        'extra_conf_files',
                        'extra_overlay_confs',
                        'extra_dtc_overlay_files',
                        'required_snippets'
                        ]:
                    extra = section.get(data)
                    if not extra:
                        continue
                    if isinstance(extra, str):
                        arg_list = extra.split(" ")
                    else:
                        arg_list = extra

                    if data == 'extra_configs':
                        args = ["-D{}".format(arg.replace('"', '\"')) for arg in arg_list]
                    elif data == 'extra_args':
                        # Retain quotes around config options
                        config_options = [arg for arg in arg_list if arg.startswith("CONFIG_")]
                        non_config_options = [arg for arg in arg_list if not arg.startswith("CONFIG_")]
                        args = ["-D{}".format(a.replace('"', '\"')) for a in config_options]
                        args.extend(["-D{}".format(arg.replace('"', '')) for arg in non_config_options])
                    elif data == 'extra_conf_files':
                        extra_conf_files.extend(arg_list)
                        continue
                    elif data == 'extra_overlay_confs':
                        extra_overlay_confs.extend(arg_list)
                        continue
                    elif data == 'extra_dtc_overlay_files':
                        extra_dtc_overlay_files.extend(arg_list)
                        continue
                    elif data == 'required_snippets':
                        required_snippets.extend(arg_list)
                        continue

                    if self.args.cmake_opts:
                        self.args.cmake_opts.extend(args)
                    else:
                        self.args.cmake_opts = args

            self.args.sysbuild = sysbuild

        if found_test_metadata:
            args = []
            if extra_conf_files:
                args.append(f"CONF_FILE=\"{';'.join(extra_conf_files)}\"")

            if extra_dtc_overlay_files:
                args.append(f"DTC_OVERLAY_FILE=\"{';'.join(extra_dtc_overlay_files)}\"")

            if extra_overlay_confs:
                args.append(f"OVERLAY_CONFIG=\"{';'.join(extra_overlay_confs)}\"")

            if required_snippets:
                args.append(f"SNIPPET=\"{';'.join(required_snippets)}\"")

            # Build the final argument list
            args_expanded = ["-D{}".format(a.replace('"', '')) for a in args]

            if self.args.cmake_opts:
                self.args.cmake_opts.extend(args_expanded)
            else:
                self.args.cmake_opts = args_expanded

        return found_test_metadata

    def _sanity_precheck(self):
        app = self.args.source_dir
        if app:
            self.check_force(
                os.path.isdir(app),
                'source directory {} does not exist'.format(app))
            self.check_force(
                'CMakeLists.txt' in os.listdir(app),
                "{} doesn't contain a CMakeLists.txt".format(app))

    def _update_cache(self):
        try:
            self.cmake_cache = CMakeCache.from_build_dir(self.build_dir)
        except FileNotFoundError:
            pass

    def _setup_build_dir(self):
        # Initialize build_dir and created_build_dir attributes.
        # If we created the build directory, we must run CMake.
        log.dbg('setting up build directory', level=log.VERBOSE_EXTREME)
        # The CMake Cache has not been loaded yet, so this is safe
        board, _ = self._find_board()
        source_dir = self._find_source_dir()
        app = os.path.split(source_dir)[1]
        build_dir = find_build_dir(self.args.build_dir, board=board,
                                   source_dir=source_dir, app=app)
        if not build_dir:
            log.die('Unable to determine a default build folder. Check '
                    'your build.dir-fmt configuration option')

        if os.path.exists(build_dir):
            if not os.path.isdir(build_dir):
                log.die('build directory {} exists and is not a directory'.
                        format(build_dir))
        else:
            os.makedirs(build_dir, exist_ok=False)
            self.created_build_dir = True
            self.run_cmake = True

        self.build_dir = build_dir

    def _find_source_dir(self):
        # Initialize source_dir attribute, either from command line argument,
        # implicitly from the build directory's CMake cache, or using the
        # default (current working directory).
        log.dbg('setting up source directory', level=log.VERBOSE_EXTREME)
        if self.args.source_dir:
            source_dir = self.args.source_dir
        elif self.cmake_cache:
            source_dir = self.cmake_cache.get('APP_DIR')

            if not source_dir:
                source_dir = self.cmake_cache.get('APPLICATION_SOURCE_DIR')

            if not source_dir:
                source_dir = self.cmake_cache.get('CMAKE_HOME_DIRECTORY')

            if not source_dir:
                # This really ought to be there. The build directory
                # must be corrupted somehow. Let's see what we can do.
                log.die('build directory', self.build_dir,
                        'CMake cache has no CMAKE_HOME_DIRECTORY;',
                        'please give a source_dir')
        else:
            source_dir = os.getcwd()
        return os.path.abspath(source_dir)

    def _sanity_check_source_dir(self):
        if self.source_dir == self.build_dir:
            # There's no forcing this.
            log.die('source and build directory {} cannot be the same; '
                    'use --build-dir {} to specify a build directory'.
                    format(self.source_dir, self.build_dir))

        srcrel = os.path.relpath(self.source_dir)
        self.check_force(
            not is_zephyr_build(self.source_dir),
            'it looks like {srcrel} is a build directory: '
            'did you mean --build-dir {srcrel} instead?'.
            format(srcrel=srcrel))
        self.check_force(
            'CMakeLists.txt' in os.listdir(self.source_dir),
            'source directory "{srcrel}" does not contain '
            'a CMakeLists.txt; is this really what you '
            'want to build? (Use -s SOURCE_DIR to specify '
            'the application source directory)'.
            format(srcrel=srcrel))

    def _sanity_check(self):
        # Sanity check the build configuration.
        # Side effect: may update cmake_cache attribute.
        log.dbg('sanity checking the build', level=log.VERBOSE_EXTREME)
        self._sanity_check_source_dir()

        if not self.cmake_cache:
            return          # That's all we can check without a cache.

        if "CMAKE_PROJECT_NAME" not in self.cmake_cache:
            # This happens sometimes when a build system is not
            # completely generated due to an error during the
            # CMake configuration phase.
            self.run_cmake = True

        cached_proj = self.cmake_cache.get('APPLICATION_SOURCE_DIR')
        cached_app = self.cmake_cache.get('APP_DIR')
        # if APP_DIR is None but APPLICATION_SOURCE_DIR is set, that indicates
        # an older build folder, this still requires pristine.
        if cached_app is None and cached_proj:
            cached_app = cached_proj

        log.dbg('APP_DIR:', cached_app, level=log.VERBOSE_EXTREME)
        source_abs = (os.path.abspath(self.args.source_dir)
                      if self.args.source_dir else None)
        cached_abs = os.path.abspath(cached_app) if cached_app else None

        log.dbg('pristine:', self.auto_pristine, level=log.VERBOSE_EXTREME)

        # If the build directory specifies a source app, make sure it's
        # consistent with --source-dir.
        apps_mismatched = (source_abs and cached_abs and
            pathlib.Path(source_abs).resolve() != pathlib.Path(cached_abs).resolve())

        self.check_force(
            not apps_mismatched or self.auto_pristine,
            'Build directory "{}" is for application "{}", but source '
            'directory "{}" was specified; please clean it, use --pristine, '
            'or use --build-dir to set another build directory'.
            format(self.build_dir, cached_abs, source_abs))

        if apps_mismatched:
            self.run_cmake = True  # If they insist, we need to re-run cmake.

        # If CACHED_BOARD is not defined, we need some other way to
        # find the board.
        cached_board = self.cmake_cache.get('CACHED_BOARD')
        log.dbg('CACHED_BOARD:', cached_board, level=log.VERBOSE_EXTREME)
        # If apps_mismatched and self.auto_pristine are true, we will
        # run pristine on the build, invalidating the cached
        # board. In that case, we need some way of getting the board.
        self.check_force((cached_board and
                          not (apps_mismatched and self.auto_pristine))
                         or self.args.board or self.config_board or
                         os.environ.get('BOARD'),
                         'Cached board not defined, please provide it '
                         '(provide --board, set default with '
                         '"west config build.board <BOARD>", or set '
                         'BOARD in the environment)')

        # Check consistency between cached board and --board.
        boards_mismatched = (self.args.board and cached_board and
                             self.args.board != cached_board)
        self.check_force(
            not boards_mismatched or self.auto_pristine,
            'Build directory {} targets board {}, but board {} was specified. '
            '(Clean the directory, use --pristine, or use --build-dir to '
            'specify a different one.)'.
            format(self.build_dir, cached_board, self.args.board))

        if self.auto_pristine and (apps_mismatched or boards_mismatched):
            self._run_pristine()
            self.cmake_cache = None
            log.dbg('run_cmake:', True, level=log.VERBOSE_EXTREME)
            self.run_cmake = True

            # Tricky corner-case: The user has not specified a build folder but
            # there was one in the CMake cache. Since this is going to be
            # invalidated, reset to CWD and re-run the basic tests.
            if ((boards_mismatched and not apps_mismatched) and
                    (not source_abs and cached_abs)):
                self.source_dir = self._find_source_dir()
                self._sanity_check_source_dir()

    def _run_cmake(self, board, origin, cmake_opts):
        if board is None and config_getboolean('board_warn', True):
            log.wrn('This looks like a fresh build and BOARD is unknown;',
                    "so it probably won't work. To fix, use",
                    '--board=<your-board>.')
            log.inf('Note: to silence the above message, run',
                    "'west config build.board_warn false'")

        if not self.run_cmake:
            return

        _banner('generating a build system')

        if board is not None and origin != 'CMakeCache.txt':
            cmake_opts = ['-DBOARD={}'.format(board)]
        else:
            cmake_opts = []
        if self.args.cmake_opts:
            cmake_opts.extend(self.args.cmake_opts)
        if self.args.snippets:
            cmake_opts.append(f'-DSNIPPET={";".join(self.args.snippets)}')
        if self.args.shields:
            cmake_opts.append(f'-DSHIELD={";".join(self.args.shields)}')

        user_args = config_get('cmake-args', None)
        if user_args:
            cmake_opts.extend(shlex.split(user_args))

        config_sysbuild = config_getboolean('sysbuild', None)

        if config_sysbuild is None:
            # If no option is set, then enable sysbuild globally
            config_sysbuild = True

        if self.args.sysbuild or (config_sysbuild and not self.args.no_sysbuild):
            cmake_opts.extend(['-S{}'.format(SYSBUILD_PROJ_DIR),
                               '-DAPP_DIR:PATH={}'.format(self.source_dir)])
        else:
            # self.args.no_sysbuild == True or config sysbuild False
            cmake_opts.extend(['-S{}'.format(self.source_dir)])

        # Invoke CMake from the current working directory using the
        # -S and -B options (officially introduced in CMake 3.13.0).
        # This is important because users expect invocations like this
        # to Just Work:
        #
        # west build -- -DOVERLAY_CONFIG=relative-path.conf
        final_cmake_args = ['-DWEST_PYTHON={}'.format(pathlib.Path(sys.executable).as_posix()),
                            '-B{}'.format(self.build_dir),
                            '-G{}'.format(config_get('generator',
                                                     DEFAULT_CMAKE_GENERATOR))]
        if cmake_opts:
            final_cmake_args.extend(cmake_opts)
        run_cmake(final_cmake_args, dry_run=self.args.dry_run)

    def _run_pristine(self):
        _banner('making build dir {} pristine'.format(self.build_dir))
        if not is_zephyr_build(self.build_dir):
            log.die('Refusing to run pristine on a folder that is not a '
                    'Zephyr build system')

        cache = CMakeCache.from_build_dir(self.build_dir)

        app_src_dir = cache.get('APPLICATION_SOURCE_DIR')
        app_bin_dir = cache.get('APPLICATION_BINARY_DIR')

        cmake_args = [f'-DBINARY_DIR={app_bin_dir}',
                      f'-DSOURCE_DIR={app_src_dir}',
                      '-P', cache['ZEPHYR_BASE'] + '/cmake/pristine.cmake']
        run_cmake(cmake_args, cwd=self.build_dir, dry_run=self.args.dry_run)

    def _run_build(self, target, domain):
        if target:
            _banner('running target {}'.format(target))
        elif self.run_cmake:
            _banner('building application')
        extra_args = ['--target', target] if target else []
        if self.args.build_opt:
            extra_args.append('--')
            extra_args.extend(self.args.build_opt)
        if self.args.verbose:
            self._append_verbose_args(extra_args,
                                      not bool(self.args.build_opt))

        domains = load_domains(self.build_dir)
        build_dir_list = []

        if domain is None:
            # If no domain is specified, we just build top build dir as that
            # will build all domains.
            build_dir_list = [domains.get_top_build_dir()]
        else:
            _banner('building domain(s): {}'.format(' '.join(domain)))
            domain_list = domains.get_domains(domain)
            for d in domain_list:
                build_dir_list.append(d.build_dir)

        for b in build_dir_list:
            run_build(b, extra_args=extra_args,
                      dry_run=self.args.dry_run)

    def _append_verbose_args(self, extra_args, add_dashes):
        # These hacks are only needed for CMake versions earlier than
        # 3.14. When Zephyr's minimum version is at least that, we can
        # drop this nonsense and just run "cmake --build BUILD -v".
        self._update_cache()
        if not self.cmake_cache:
            return
        generator = self.cmake_cache.get('CMAKE_GENERATOR')
        if not generator:
            return
        # Substring matching is for things like "Eclipse CDT4 - Ninja".
        if 'Ninja' in generator:
            if add_dashes:
                extra_args.append('--')
            extra_args.append('-v')
        elif generator == 'Unix Makefiles':
            if add_dashes:
                extra_args.append('--')
            extra_args.append('VERBOSE=1')
