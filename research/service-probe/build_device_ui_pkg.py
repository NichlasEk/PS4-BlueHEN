#!/usr/bin/env python3
"""Build the isolated BLHN00007 module test using a local OpenOrbis toolchain.

System runtime modules are copied locally and must not be committed.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
from auth_profile import validate_profile, verify_signature
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
sdk = Path(os.environ.get('OO_PS4_TOOLCHAIN', '/opt/openorbis/OpenOrbis/PS4Toolchain'))
out = root / 'build/service-ui-v18'
stage = out / 'stage'
out.mkdir(parents=True, exist_ok=False)
stage.mkdir()
# Profile inputs stay private; no source-tree credential templates.
baseline = Path(os.environ['BLUEHEN_BASELINE_AUTH']).read_bytes()
profile = Path(os.environ['BLUEHEN_PROFILE_AUTH']).read_bytes()
paid = validate_profile(baseline, profile)
header = out / 'expected_auth.h'
header.write_text('static const unsigned char bluehen_expected_auth[136] = {' +
                  ','.join(str(x) for x in profile) + '};\n')
env = dict(os.environ, OO_PS4_TOOLCHAIN=str(sdk), DOTNET_SYSTEM_GLOBALIZATION_INVARIANT='1')
# Keep the legacy single-file .NET extractor away from stale shared caches.
temporary = out / 'dotnet-tmp'
temporary.mkdir(exist_ok=True)
env['TMPDIR'] = str(temporary)
if os.environ.get('LIBSSL11'):
    env['LD_LIBRARY_PATH'] = os.environ['LIBSSL11']


def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, env=env, **kwargs)


run('clang', '--target=x86_64-pc-freebsd12-elf', '-fPIC', '-O2', '-Wall', '-Wextra',
    '-Werror', '-isysroot', sdk, '-isystem', sdk / 'include',
    '-DBLUEHEN_LOG="/data/bluehen/service-ui-v18.log"',
    '-include', header, '-c', root / 'research/service-probe/main_device_ui.c', '-o', out / 'main.o')
run('ld.lld', '-m', 'elf_x86_64', '-pie', '--script=' + str(sdk / 'link.x'),
    '--eh-frame-hdr', '-z', 'max-page-size=0x4000', '-L' + str(sdk / 'lib'),
    sdk / 'lib/crt1.o', out / 'main.o', '-lSDL2', '-lc', '-lkernel', '-lc++', '-lSceUserService', '-lSceVideoOut', '-lSceAudioOut', '-lScePad', '-lSceSysmodule', '-o', out / 'app.elf')
run(os.environ.get('CREATE_FSELF', str(sdk / 'bin/linux/create-fself')),
    '-in=' + str(out / 'app.elf'), '-out=' + str(out / 'app.oelf'),
    '--eboot', stage / 'eboot.bin', '--paid', hex(paid), '--authinfo', profile.hex())
# Verify the exact fake-SELF signature body before packaging.
eboot = (stage / 'eboot.bin').read_bytes()
verify_signature(eboot, profile)
files = ['eboot.bin', 'sce_sys/param.sfo', 'sce_sys/icon0.png',
         'sce_sys/about/right.sprx', 'sce_module/libc.prx', 'sce_module/libSceFios2.prx']
for name, source in {
    'sce_sys/icon0.png': root / 'launcher/sce_sys/icon0.png',
    'sce_sys/about/right.sprx': sdk / 'samples/dialogs/sce_sys/about/right.sprx',
    'sce_module/libc.prx': sdk / 'src/modules/libc.prx',
    'sce_module/libSceFios2.prx': sdk / 'src/modules/libSceFios2.prx',
}.items():
    (stage / name).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, stage / name)
content = 'IV0000-BLHN00007_00-BLUEHENSERVICE01'
tool = sdk / 'bin/linux/PkgTool.Core'
sfo = stage / 'sce_sys/param.sfo'
sfo.unlink(missing_ok=True)
run(tool, 'sfo_new', sfo)
for key, value in dict(APP_TYPE=1, ATTRIBUTE=0, DOWNLOAD_DATA_SIZE=0, SYSTEM_VER=0).items():
    run(tool, 'sfo_setentry', sfo, key, '--type', 'Integer', '--maxsize', 4, '--value', value)
for key, size, value in [('APP_VER', 8, '00.18'), ('VERSION', 8, '01.00'),
                         ('CATEGORY', 4, 'gd'), ('CONTENT_ID', 48, content),
                         ('TITLE_ID', 12, 'BLHN00007'), ('TITLE', 128, 'BlueHEN Bluetooth')]:
    run(tool, 'sfo_setentry', sfo, key, '--type', 'Utf8', '--maxsize', size, '--value', value)
run(sdk / 'bin/linux/create-gp4', '-out', 'pkg.gp4', '--content-id=' + content,
    '--files', ' '.join(files), cwd=stage)
tree = ET.parse(stage / 'pkg.gp4')
directory = tree.getroot().find('rootdir')
directory.clear()
for name in files:
    node = directory
    for part in Path(name).parts[:-1]:
        child = next((c for c in node if c.get('targ_name') == part), None)
        if child is None:
            child = ET.SubElement(node, 'dir', targ_name=part)
        node = child
tree.write(stage / 'pkg.gp4', encoding='utf-8', xml_declaration=True)
run(tool, 'pkg_build', 'pkg.gp4', '..', cwd=stage)
pkg = out / (content + '.pkg')
with (out / 'validation.log').open('w') as log:
    run(tool, 'pkg_validate', '--verbose', pkg, stdout=log, stderr=subprocess.STDOUT)
if '[OK]' not in (out / 'validation.log').read_text():
    raise RuntimeError('Package validation did not pass')
manifest = dict(profile_sha256=hashlib.sha256(profile).hexdigest(),
                baseline_sha256=hashlib.sha256(baseline).hexdigest(), title_id='BLHN00007', bytes=pkg.stat().st_size,
                sha256=hashlib.sha256(pkg.read_bytes()).hexdigest(), package=str(pkg))
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest))
