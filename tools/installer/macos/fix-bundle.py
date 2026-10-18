#!/usr/bin/env python3
#
# fix-bundle.py <App.app> [prefix]
#
# After macdeployqt: points every library reference in the bundle that still names the build machine's Qt and
# its libraries (MacPorts' /opt/local by default) at the copy inside the bundle, copying in anything that isn't
# there yet. macdeployqt handles the main executable and the frameworks it deploys, but not the helper
# executables inside them, such as QtWebEngineCore's QtWebEngineProcess, when the Qt it copies from uses absolute
# install names, as MacPorts' does. References become @loader_path-relative, so they hold wherever the app goes.
#
# Changing load commands breaks code signatures: sign the bundle again afterwards.

import os
import shutil
import subprocess
import sys


EXECUTABLE_FRAMEWORKS = '@executable_path/../Frameworks/'


def is_macho(path):
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
    except OSError:
        return False
    return magic in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')


def dependencies(path):
    out = subprocess.run(['otool', '-L', path], stdout=subprocess.PIPE, universal_newlines=True, check=True).stdout
    ident = subprocess.run(['otool', '-D', path], stdout=subprocess.PIPE, universal_newlines=True).stdout.splitlines()
    ident = ident[1].strip() if len(ident) > 1 else None
    deps = []
    for line in out.splitlines()[1:]:
        line = line.strip()
        if line:
            dep = line.split(' (compatibility')[0].strip()
            if dep != ident:
                deps.append(dep)
    return ident, deps


def bundle_target(frameworks, dep):
    # where a dependency lives inside the bundle, and what to copy there if it's missing
    if '.framework/' in dep:
        start = dep.rindex('/', 0, dep.index('.framework/')) + 1
        name = dep[start:dep.index('.framework/') + len('.framework')]
        return os.path.join(frameworks, dep[start:]), os.path.join(dep[:start], name), os.path.join(frameworks, name)
    return os.path.join(frameworks, os.path.basename(dep)), dep, os.path.join(frameworks, os.path.basename(dep))


def main():
    app = os.path.abspath(sys.argv[1])
    prefix = sys.argv[2] if len(sys.argv) > 2 else '/opt/local/'
    frameworks = os.path.join(app, 'Contents', 'Frameworks')
    os.makedirs(frameworks, exist_ok=True)

    queue = []
    for root, _, files in os.walk(app):
        for name in files:
            path = os.path.join(root, name)
            if not os.path.islink(path) and is_macho(path):
                queue.append(path)

    done = set()
    changed = 0
    while queue:
        path = queue.pop()
        if path in done:
            continue
        done.add(path)
        ident, deps = dependencies(path)
        for dep in deps:
            # macdeployqt links the frameworks it copies to each other as @executable_path/../Frameworks/...,
            # which only resolves from the main executable: QtWebEngineProcess, deep inside QtWebEngineCore,
            # then can't load Qt. Relative to each file instead, it resolves from any executable.
            if dep.startswith(EXECUTABLE_FRAMEWORKS):
                target = os.path.join(frameworks, dep[len(EXECUTABLE_FRAMEWORKS):])
                if not os.path.exists(target):
                    sys.exit('{} needs {}, which is not in the bundle'.format(path, dep))
                relative = os.path.relpath(os.path.realpath(target), os.path.dirname(os.path.realpath(path)))
                subprocess.run(['install_name_tool', '-change', dep, '@loader_path/' + relative, path],
                               check=True, stderr=subprocess.DEVNULL)
                changed += 1
                continue
            if not dep.startswith(prefix):
                continue
            target, source, copy_to = bundle_target(frameworks, dep)
            if not os.path.exists(target):
                print('Copying in {} (for {})'.format(source, os.path.relpath(path, app)))
                if os.path.isdir(source):
                    subprocess.run(['ditto', source, copy_to], check=True)
                    for root, _, files in os.walk(copy_to):
                        for name in files:
                            p = os.path.join(root, name)
                            if not os.path.islink(p) and is_macho(p):
                                queue.append(p)
                else:
                    shutil.copy2(source, copy_to)
                    os.chmod(copy_to, 0o755)
                    queue.append(copy_to)
            if not os.path.exists(target):
                sys.exit('{} needs {}, which is not in the bundle and could not be copied in'.format(path, dep))
            relative = os.path.relpath(os.path.realpath(target), os.path.dirname(os.path.realpath(path)))
            subprocess.run(['install_name_tool', '-change', dep, '@loader_path/' + relative, path],
                           check=True, stderr=subprocess.DEVNULL)
            changed += 1
        if ident and ident.startswith(prefix):
            subprocess.run(['install_name_tool', '-id', '@rpath/' + os.path.basename(ident), path],
                           check=True, stderr=subprocess.DEVNULL)

    print('Repointed {} references to {}'.format(changed, prefix))


if __name__ == '__main__':
    main()
