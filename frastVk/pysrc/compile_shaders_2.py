import os,sys,subprocess,re
from argparse import ArgumentParser

# targetEnv = '--target-env vulkan1.0'
targetEnv = ''

def compileSource(sourceFile, type, path=""):
    if path == "": path = "/tmp/shader_" + str(os.getpid())
    with open(sourceFile,'r') as fp: source = fp.read()


    result = re.search('##include.+"([a-zA-Z.0-9]+)"', source)
    while result:
        includeName = result.group(1)
        searchPath = os.path.join(sourceFile.rsplit('/', 1)[0], includeName)
        print(' INCLUDE NAME', includeName)
        with open(searchPath, 'r') as fp: newSource = fp.read()
        source = source[:result.span()[0]] + newSource + source[result.span()[1]:]
        #print(' Modified source:\n', source)
        result = re.search('##include.+"([a-zA-Z.0-9]+)"', source)

    # cmd = "glslangValidator --stdin -S {} -V -o {} << END\n".format(type, path) + source + "\nEND"
    # cmd = "glslangValidator --stdin -S {} -V -o {} {} << END\n".format(type, path, targetEnv) + source + "\nEND"
    cmd = "glslangValidator --stdin -e main -S {} -o {} {} << END\n".format(type, path, targetEnv) + source + "\nEND"

    res = subprocess.getoutput(cmd)
    if len(res) > 10:
        print(' - Shader from \'{}\' may have failed, output:\n'.format(sourceFile), res)
        return ('', 0)

    #print(' - Result:', res)
    with open(path,'rb') as fp:
        bs = fp.read()
    # bs = bs + bytes([0])
    codeLen = len(bs)

    return ''.join(('\\x{}'.format(hex(b)[2:]) for b in bs)), codeLen

    out = ''
    for i in range(len(bs)//4):
        for j in range(4):
            out += '\\x' + hex(bs[i*4+(3-j)])[2:]
    return (out, codeLen)

# Compile the given file, return its name-key and tuple of <spirv, len>
def doWork(file):
    print(' - On',file)
    #name = '_'.join(file.rsplit('/',2)[-2:]).replace('.','_')
    # name = '/'.join(file.rsplit('/',2)[-2:])
    name = '/'.join(file.split('/')[2:])

    type = 'unk'
    if '.f.glsl' in file:
        type = 'frag'
    elif '.v.glsl' in file:
        type = 'vert'
    elif '.c.glsl' in file:
        type = 'comp'
    elif '.rg.glsl' in file:
        type = 'rgen'
    elif '.rch.glsl' in file:
        type = 'rchit'
    elif '.rah.glsl' in file:
        type = 'rahit'
    elif '.rm.glsl' in file:
        type = 'rmiss'
    elif '.rcall.glsl' in file:
        type = 'rcall'
    else:
        print('uknown file ext:', file)

    # spirvs[name] = compileSource(file, type)
    if type != 'unk':
        return name, compileSource(file, type)

def main():
    parser = ArgumentParser()

    parser.add_argument('--srcFiles', required=True, nargs='+')
    parser.add_argument('--dstName', required=True)
    parser.add_argument('--namespace', required=True)
    parser.add_argument('--targetEnv', default='--target-env=vulkan1.0') # For raytracing, must pass "--targetEnv='--target-env spirv1.5'"
    args = parser.parse_args()
    global targetEnv
    targetEnv = args.targetEnv

    spirvs = {}



    # Prefer multithreaded version
    if False:
        for file in args.srcFiles:
            name, tup = doWork(file)
            spirvs[name] = tup
    else:
        from multiprocessing import Pool
        with Pool(6) as pool:
            res = pool.map(doWork, args.srcFiles)
            for name_tup in res:
                if name_tup:
                    name, tup = name_tup
                    spirvs[name] = tup

    N = len(spirvs)
    namespace = args.namespace

    hdr_code = '''
#include <frastVk/core/fvkShaders.h>

namespace {namespace} {{
using FvkShaderGroup_ = FvkShaderGroup<{N}>;
extern FvkShaderGroup_ shaderGroup;
}}
'''.format(N=N, namespace=namespace)


    with open(args.dstName + '.h', 'w') as fp:
        fp.write('#pragma once\n')
        fp.write('#include <array>\n\n')
        fp.write(hdr_code)
    with open(args.dstName + '.cc', 'w') as fp:
        fp.write('#include <cstring>\n')
        fp.write('#include <string>\n')
        fp.write('#include <cstdint>\n')
        fp.write('#include <array>\n')
        fp.write('#include "{}"\n'.format(args.dstName + '.h'))

        fp.write('namespace {namespace} {{\n'.format(namespace=namespace))

        fp.write('FvkShaderGroup_ shaderGroup'.format(len(spirvs)) + ' = {' + '\n')
        fp.write('  .shaders = std::array<std::pair<std::string, std::string>, {N}> {{ {{'.format(N=N))
        for i,(name,(code,codeLen)) in enumerate(spirvs.items()):
            fp.write(' { ' + '"{}", '.format(name))
            # fp.write(code[:2])
            fp.write('std::string{"')
            fp.write(code)
            fp.write('",' + str(codeLen) + '}')
            if i != len(spirvs)-1:
                fp.write(' },\n')
            else:
                fp.write(' }\n')
        fp.write('} } };\n')

        fp.write('}')



    print(' - Wrote {}'.format(args.dstName))

if __name__ == '__main__': main()

