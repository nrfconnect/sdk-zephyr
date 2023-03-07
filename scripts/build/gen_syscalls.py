#!/usr/bin/env python3
#
# Copyright (c) 2017 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

"""
Script to generate system call invocation macros

This script parses the system call metadata JSON file emitted by
parse_syscalls.py to create several files:

- A file containing weak aliases of any potentially unimplemented system calls,
  as well as the system call dispatch table, which maps system call type IDs
  to their handler functions.

- A header file defining the system call type IDs, as well as function
  prototypes for all system call handler functions.

- A directory containing header files. Each header corresponds to a header
  that was identified as containing system call declarations. These
  generated headers contain the inline invocation functions for each system
  call in that header.
"""

import sys
import re
import argparse
import os
import json

# Some kernel headers cannot include automated tracing without causing unintended recursion or
# other serious issues.
# These headers typically already have very specific tracing hooks for all relevant things
# written by hand so are excluded.
notracing = ["kernel.h", "zephyr/kernel.h", "errno_private.h",
             "zephyr/errno_private.h"]

types64 = ["int64_t", "uint64_t"]

# The kernel linkage is complicated.  These functions from
# userspace_handlers.c are present in the kernel .a library after
# userspace.c, which contains the weak fallbacks defined here.  So the
# linker finds the weak one first and stops searching, and thus won't
# see the real implementation which should override.  Yet changing the
# order runs afoul of a comment in CMakeLists.txt that the order is
# critical.  These are core syscalls that won't ever be unconfigured,
# just disable the fallback mechanism as a simple workaround.
noweak = ["z_mrsh_k_object_release",
          "z_mrsh_k_object_access_grant",
          "z_mrsh_k_object_alloc"]

table_template = """/* auto-generated by gen_syscalls.py, don't edit */

/* Weak handler functions that get replaced by the real ones unless a system
 * call is not implemented due to kernel configuration.
 */
%s

const _k_syscall_handler_t _k_syscall_table[K_SYSCALL_LIMIT] = {
\t%s
};
"""

list_template = """/* auto-generated by gen_syscalls.py, don't edit */

#ifndef ZEPHYR_SYSCALL_LIST_H
#define ZEPHYR_SYSCALL_LIST_H

%s

#ifndef _ASMLANGUAGE

#include <stdint.h>

#endif /* _ASMLANGUAGE */

#endif /* ZEPHYR_SYSCALL_LIST_H */
"""

syscall_template = """/* auto-generated by gen_syscalls.py, don't edit */

{include_guard}

{tracing_include}

#ifndef _ASMLANGUAGE

#include <syscall_list.h>
#include <zephyr/syscall.h>

#include <zephyr/linker/sections.h>


#ifdef __cplusplus
extern "C" {{
#endif

{invocations}

#ifdef __cplusplus
}}
#endif

#endif
#endif /* include guard */
"""

handler_template = """
extern uintptr_t z_hdlr_%s(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, void *ssf);
"""

weak_template = """
__weak ALIAS_OF(handler_no_syscall)
uintptr_t %s(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
         uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, void *ssf);
"""

# defines a macro wrapper which supersedes the syscall when used
# and provides tracing enter/exit hooks while allowing per compilation unit
# enable/disable of syscall tracing. Used for returning functions
# Note that the last argument to the exit macro is the return value.
syscall_tracer_with_return_template = """
#if defined(CONFIG_TRACING_SYSCALL)
#ifndef DISABLE_SYSCALL_TRACING
{trace_diagnostic}
#define {func_name}({argnames}) ({{ \
	{func_type} retval; \
	sys_port_trace_syscall_enter({syscall_id}, {func_name}{trace_argnames}); \
	retval = {func_name}({argnames}); \
	sys_port_trace_syscall_exit({syscall_id}, {func_name}{trace_argnames}, retval); \
	retval; \
}})
#endif
#endif
"""

# defines a macro wrapper which supersedes the syscall when used
# and provides tracing enter/exit hooks while allowing per compilation unit
# enable/disable of syscall tracing. Used for non-returning (void) functions
syscall_tracer_void_template = """
#if defined(CONFIG_TRACING_SYSCALL)
#ifndef DISABLE_SYSCALL_TRACING
{trace_diagnostic}
#define {func_name}({argnames}) do {{ \
	sys_port_trace_syscall_enter({syscall_id}, {func_name}{trace_argnames}); \
	{func_name}({argnames}); \
	sys_port_trace_syscall_exit({syscall_id}, {func_name}{trace_argnames}); \
}} while(false)
#endif
#endif
"""

typename_regex = re.compile(r'(.*?)([A-Za-z0-9_]+)$')


class SyscallParseException(Exception):
    pass


def typename_split(item):
    if "[" in item:
        raise SyscallParseException(
            "Please pass arrays to syscalls as pointers, unable to process '%s'" %
            item)

    if "(" in item:
        raise SyscallParseException(
            "Please use typedefs for function pointers")

    mo = typename_regex.match(item)
    if not mo:
        raise SyscallParseException("Malformed system call invocation")

    m = mo.groups()
    return (m[0].strip(), m[1])

def need_split(argtype):
    return (not args.long_registers) and (argtype in types64)

# Note: "lo" and "hi" are named in little endian conventions,
# but it doesn't matter as long as they are consistently
# generated.
def union_decl(type, split):
    middle = "struct { uintptr_t lo, hi; } split" if split else "uintptr_t x"
    return "union { %s; %s val; }" % (middle, type)

def wrapper_defs(func_name, func_type, args, fn):
    ret64 = need_split(func_type)
    mrsh_args = [] # List of rvalue expressions for the marshalled invocation

    decl_arglist = ", ".join([" ".join(argrec) for argrec in args]) or "void"
    syscall_id = "K_SYSCALL_" + func_name.upper()

    wrap = "extern %s z_impl_%s(%s);\n" % (func_type, func_name, decl_arglist)
    wrap += "\n"
    wrap += "__pinned_func\n"
    wrap += "static inline %s %s(%s)\n" % (func_type, func_name, decl_arglist)
    wrap += "{\n"
    wrap += "#ifdef CONFIG_USERSPACE\n"
    wrap += ("\t" + "uint64_t ret64;\n") if ret64 else ""
    wrap += "\t" + "if (z_syscall_trap()) {\n"

    valist_args = []
    for argnum, (argtype, argname) in enumerate(args):
        split = need_split(argtype)
        wrap += "\t\t%s parm%d" % (union_decl(argtype, split), argnum)
        if argtype != "va_list":
            wrap += " = { .val = %s };\n" % argname
        else:
            # va_list objects are ... peculiar.
            wrap += ";\n" + "\t\t" + "va_copy(parm%d.val, %s);\n" % (argnum, argname)
            valist_args.append("parm%d.val" % argnum)
        if split:
            mrsh_args.append("parm%d.split.lo" % argnum)
            mrsh_args.append("parm%d.split.hi" % argnum)
        else:
            mrsh_args.append("parm%d.x" % argnum)

    if ret64:
        mrsh_args.append("(uintptr_t)&ret64")

    if len(mrsh_args) > 6:
        wrap += "\t\t" + "uintptr_t more[] = {\n"
        wrap += "\t\t\t" + (",\n\t\t\t".join(mrsh_args[5:])) + "\n"
        wrap += "\t\t" + "};\n"
        mrsh_args[5:] = ["(uintptr_t) &more"]

    invoke = ("arch_syscall_invoke%d(%s)"
              % (len(mrsh_args),
                 ", ".join(mrsh_args + [syscall_id])))

    if ret64:
        invoke = "\t\t" + "(void) %s;\n" % invoke
        retcode = "\t\t" + "return (%s) ret64;\n" % func_type
    elif func_type == "void":
        invoke = "\t\t" + "(void) %s;\n" % invoke
        retcode = "\t\t" + "return;\n"
    elif valist_args:
        invoke = "\t\t" + "%s retval = %s;\n" % (func_type, invoke)
        retcode = "\t\t" + "return retval;\n"
    else:
        invoke = "\t\t" + "return (%s) %s;\n" % (func_type, invoke)
        retcode = ""

    wrap += invoke
    for argname in valist_args:
        wrap += "\t\t" + "va_end(%s);\n" % argname
    wrap += retcode
    wrap += "\t" + "}\n"
    wrap += "#endif\n"

    # Otherwise fall through to direct invocation of the impl func.
    # Note the compiler barrier: that is required to prevent code from
    # the impl call from being hoisted above the check for user
    # context.
    impl_arglist = ", ".join([argrec[1] for argrec in args])
    impl_call = "z_impl_%s(%s)" % (func_name, impl_arglist)
    wrap += "\t" + "compiler_barrier();\n"
    wrap += "\t" + "%s%s;\n" % ("return " if func_type != "void" else "",
                               impl_call)

    wrap += "}\n"

    if fn not in notracing:
        argnames = ", ".join([f"{argname}" for _, argname in args])
        trace_argnames = ""
        if len(args) > 0:
            trace_argnames = ", " + argnames
        trace_diagnostic = ""
        if os.getenv('TRACE_DIAGNOSTICS'):
            trace_diagnostic = f"#warning Tracing {func_name}"
        if func_type != "void":
            wrap += syscall_tracer_with_return_template.format(func_type=func_type, func_name=func_name,
                                                               argnames=argnames, trace_argnames=trace_argnames,
                                                               syscall_id=syscall_id, trace_diagnostic=trace_diagnostic)
        else:
            wrap += syscall_tracer_void_template.format(func_type=func_type, func_name=func_name,
                                                        argnames=argnames, trace_argnames=trace_argnames,
                                                        syscall_id=syscall_id, trace_diagnostic=trace_diagnostic)

    return wrap

# Returns an expression for the specified (zero-indexed!) marshalled
# parameter to a syscall, with handling for a final "more" parameter.
def mrsh_rval(mrsh_num, total):
    if mrsh_num < 5 or total <= 6:
        return "arg%d" % mrsh_num
    else:
        return "(((uintptr_t *)more)[%d])" % (mrsh_num - 5)

def marshall_defs(func_name, func_type, args):
    mrsh_name = "z_mrsh_" + func_name

    nmrsh = 0        # number of marshalled uintptr_t parameter
    vrfy_parms = []  # list of (argtype, bool_is_split)
    for (argtype, _) in args:
        split = need_split(argtype)
        vrfy_parms.append((argtype, split))
        nmrsh += 2 if split else 1

    # Final argument for a 64 bit return value?
    if need_split(func_type):
        nmrsh += 1

    decl_arglist = ", ".join([" ".join(argrec) for argrec in args])
    mrsh = "extern %s z_vrfy_%s(%s);\n" % (func_type, func_name, decl_arglist)

    mrsh += "uintptr_t %s(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,\n" % mrsh_name
    if nmrsh <= 6:
        mrsh += "\t\t" + "uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, void *ssf)\n"
    else:
        mrsh += "\t\t" + "uintptr_t arg3, uintptr_t arg4, void *more, void *ssf)\n"
    mrsh += "{\n"
    mrsh += "\t" + "_current->syscall_frame = ssf;\n"

    for unused_arg in range(nmrsh, 6):
        mrsh += "\t(void) arg%d;\t/* unused */\n" % unused_arg

    if nmrsh > 6:
        mrsh += ("\tZ_OOPS(Z_SYSCALL_MEMORY_READ(more, "
                 + str(nmrsh - 5) + " * sizeof(uintptr_t)));\n")

    argnum = 0
    for i, (argtype, split) in enumerate(vrfy_parms):
        mrsh += "\t%s parm%d;\n" % (union_decl(argtype, split), i)
        if split:
            mrsh += "\t" + "parm%d.split.lo = %s;\n" % (i, mrsh_rval(argnum, nmrsh))
            argnum += 1
            mrsh += "\t" + "parm%d.split.hi = %s;\n" % (i, mrsh_rval(argnum, nmrsh))
        else:
            mrsh += "\t" + "parm%d.x = %s;\n" % (i, mrsh_rval(argnum, nmrsh))
        argnum += 1

    # Finally, invoke the verify function
    out_args = ", ".join(["parm%d.val" % i for i in range(len(args))])
    vrfy_call = "z_vrfy_%s(%s)" % (func_name, out_args)

    if func_type == "void":
        mrsh += "\t" + "%s;\n" % vrfy_call
        mrsh += "\t" + "_current->syscall_frame = NULL;\n"
        mrsh += "\t" + "return 0;\n"
    else:
        mrsh += "\t" + "%s ret = %s;\n" % (func_type, vrfy_call)

        if need_split(func_type):
            ptr = "((uint64_t *)%s)" % mrsh_rval(nmrsh - 1, nmrsh)
            mrsh += "\t" + "Z_OOPS(Z_SYSCALL_MEMORY_WRITE(%s, 8));\n" % ptr
            mrsh += "\t" + "*%s = ret;\n" % ptr
            mrsh += "\t" + "_current->syscall_frame = NULL;\n"
            mrsh += "\t" + "return 0;\n"
        else:
            mrsh += "\t" + "_current->syscall_frame = NULL;\n"
            mrsh += "\t" + "return (uintptr_t) ret;\n"

    mrsh += "}\n"

    return mrsh, mrsh_name

def analyze_fn(match_group, fn):
    func, args = match_group

    try:
        if args == "void":
            args = []
        else:
            args = [typename_split(a.strip()) for a in args.split(",")]

        func_type, func_name = typename_split(func)
    except SyscallParseException:
        sys.stderr.write("In declaration of %s\n" % func)
        raise

    sys_id = "K_SYSCALL_" + func_name.upper()

    marshaller = None
    marshaller, handler = marshall_defs(func_name, func_type, args)
    invocation = wrapper_defs(func_name, func_type, args, fn)

    # Entry in _k_syscall_table
    table_entry = "[%s] = %s" % (sys_id, handler)

    return (handler, invocation, marshaller, sys_id, table_entry)

def parse_args():
    global args
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)

    parser.add_argument("-i", "--json-file", required=True,
                        help="Read syscall information from json file")
    parser.add_argument("-d", "--syscall-dispatch", required=True,
                        help="output C system call dispatch table file")
    parser.add_argument("-l", "--syscall-list", required=True,
                        help="output C system call list header")
    parser.add_argument("-o", "--base-output", required=True,
                        help="Base output directory for syscall macro headers")
    parser.add_argument("-s", "--split-type", action="append",
                        help="A long type that must be split/marshalled on 32-bit systems")
    parser.add_argument("-x", "--long-registers", action="store_true",
                        help="Indicates we are on system with 64-bit registers")
    args = parser.parse_args()


def main():
    parse_args()

    if args.split_type is not None:
        for t in args.split_type:
            types64.append(t)

    with open(args.json_file, 'r') as fd:
        syscalls = json.load(fd)

    invocations = {}
    mrsh_defs = {}
    mrsh_includes = {}
    ids = []
    table_entries = []
    handlers = []

    for match_group, fn in syscalls:
        handler, inv, mrsh, sys_id, entry = analyze_fn(match_group, fn)

        if fn not in invocations:
            invocations[fn] = []

        invocations[fn].append(inv)
        ids.append(sys_id)
        table_entries.append(entry)
        handlers.append(handler)

        if mrsh:
            syscall = typename_split(match_group[0])[1]
            mrsh_defs[syscall] = mrsh
            mrsh_includes[syscall] = "#include <syscalls/%s>" % fn

    with open(args.syscall_dispatch, "w") as fp:
        table_entries.append("[K_SYSCALL_BAD] = handler_bad_syscall")

        weak_defines = "".join([weak_template % name
                                for name in handlers
                                if not name in noweak])

        # The "noweak" ones just get a regular declaration
        weak_defines += "\n".join(["extern uintptr_t %s(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, void *ssf);"
                                   % s for s in noweak])

        fp.write(table_template % (weak_defines,
                                   ",\n\t".join(table_entries)))

    # Listing header emitted to stdout
    ids.sort()
    ids.extend(["K_SYSCALL_BAD", "K_SYSCALL_LIMIT"])

    ids_as_defines = ""
    for i, item in enumerate(ids):
        ids_as_defines += "#define {} {}\n".format(item, i)

    with open(args.syscall_list, "w") as fp:
        fp.write(list_template % ids_as_defines)

    os.makedirs(args.base_output, exist_ok=True)
    for fn, invo_list in invocations.items():
        out_fn = os.path.join(args.base_output, fn)

        ig = re.sub("[^a-zA-Z0-9]", "_", "Z_INCLUDE_SYSCALLS_" + fn).upper()
        include_guard = "#ifndef %s\n#define %s\n" % (ig, ig)
        tracing_include = ""
        if fn not in notracing:
            tracing_include = "#include <zephyr/tracing/tracing_syscall.h>"
        header = syscall_template.format(include_guard=include_guard, tracing_include=tracing_include, invocations="\n\n".join(invo_list))

        with open(out_fn, "w") as fp:
            fp.write(header)

    # Likewise emit _mrsh.c files for syscall inclusion
    for fn in mrsh_defs:
        mrsh_fn = os.path.join(args.base_output, fn + "_mrsh.c")

        with open(mrsh_fn, "w") as fp:
            fp.write("/* auto-generated by gen_syscalls.py, don't edit */\n\n")
            fp.write(mrsh_includes[fn] + "\n")
            fp.write("\n")
            fp.write(mrsh_defs[fn] + "\n")

if __name__ == "__main__":
    main()
