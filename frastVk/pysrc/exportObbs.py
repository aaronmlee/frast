import sys, os, numpy as np
from .transformAuthalicToGeodetic import authalic_to_geodetic_corners, authalic_to_wgs84_pt, EarthGeodetic
from .proto import rocktree_pb2 as RT

# Version 1 Format:
#     - Just a bunch of rows with binary data
#              - The first 26 bytes are the node's key. The left-most bytes have digits 0-8. The right-most are filled 255 to indicate invalid
#              - Next are 10 floats (3 + 3 + 4) for (pos extent quat)

# https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
def R_to_quat(R):
    tr = np.trace(R)
    if tr > 0:
        S = np.sqrt(tr+1) * 2
        q =  np.array((
            .25*S,
            (R[2,1] - R[1,2]) / S,
            (R[0,2] - R[2,0]) / S,
            (R[1,0] - R[0,1]) / S))
    elif (R[0,0]>R[1,1]) and (R[0,0]>R[2,2]):
        S = np.sqrt(1 + R[0,0]-R[1,1]-R[2,2]) * 2
        q =  np.array((
            (R[2,1] - R[1,2]) / S,
            .25*S,
            (R[0,1] + R[1,0]) / S,
            (R[0,2] + R[2,0]) / S))
    elif R[1,1]>R[2,2]:
        S = np.sqrt(1 + R[1,1]-R[0,0]-R[2,2]) * 2
        q =  np.array((
            (R[0,2] - R[2,0]) / S,
            (R[0,1] + R[1,0]) / S,
            .25*S,
            (R[1,2] + R[2,1]) / S))
    else:
        S = np.sqrt(1 + R[2,2]-R[0,0]-R[1,1]) * 2
        q =  np.array((
            (R[1,0] - R[0,1]) / S,
            (R[0,2] + R[2,0]) / S,
            (R[1,2] + R[2,1]) / S),
            .25*S)
    return q / np.linalg.norm(q)


truth_table_ = np.array((
    0,0,0,
    0,0,1,
    0,1,0,
    0,1,1,
    1,0,0,
    1,0,1,
    1,1,0,
    1,1,1)).reshape(-1,3)[:, [2,1,0]]
def truth_table():
    global truth_table_
    return truth_table_

def rt_get_level_and_path_and_flags(path_and_flags):
    level = 1 + (path_and_flags & 3)
    path_and_flags >>= 2
    path = ''
    for i in range(level):
        path += chr(ord('0') + (path_and_flags & 7))
        path_and_flags >>= 3
    # while len(path) % 4 != 0: path = path + '0'
    flags = path_and_flags
    return level, path, flags

def rt_decode_obb(bites, headNodeCtr, metersPerTexel):
    assert len(bites) == 15
    data = np.frombuffer(bites,dtype=np.uint8)

    ctr = np.frombuffer(bites[:6], dtype=np.int16).astype(np.float32) * metersPerTexel + headNodeCtr
    ext = np.frombuffer(bites[6:9], dtype=np.uint8).astype(np.float32) * metersPerTexel

    euler = np.frombuffer(bites[9:], dtype=np.uint16) * (np.pi/32768, np.pi/65536, np.pi/32768)
    c0 = np.cos(euler[0])
    s0 = np.sin(euler[0])
    c1 = np.cos(euler[1])
    s1 = np.sin(euler[1])
    c2 = np.cos(euler[2])
    s2 = np.sin(euler[2])

    return ctr, ext, np.array((
        c0*c2-c1*s0*s2, c1*c0*s2+c2*s0, s2*s1,
        -c0*s2-c2*c1*s0, c0*c1*c2-s0*s2, c2*s1,
        s1*s0, -c0*s1, c1), dtype=np.float32).reshape(3,3)

def export_rt_version1(outFp, root, transformToWGS84=True):

    bulkDir = os.path.join(root, 'bulk')
    nodeDir = os.path.join(root, 'node')

    nodeSet = set()
    for fi in os.listdir(nodeDir):
        nodeSet.add(fi)
    print(' - Have {} nodes'.format(len(nodeSet)))

    bulks = os.listdir(bulkDir)
    print(' - Have {} bulks'.format(len(bulks)))
    nodesSeen = 0

    for i,fi in enumerate(bulks):
        if i % 1000 == 0: print(' - bulk {} ({} / {} nodes)'.format(i, nodesSeen, len(nodeSet)))
        filename = os.path.join(bulkDir, fi)

        bulkPath = fi.split('_')[0]

        with open(filename,'rb') as fp:
            bulk = RT.BulkMetadata.FromString(fp.read())

            head_center = bulk.head_node_center
            mtt_per_level = bulk.meters_per_texel

            for i in range(len(bulk.node_metadata)):
                rlevel, rpath, flags = rt_get_level_and_path_and_flags(bulk.node_metadata[i].path_and_flags)

                path = bulkPath + rpath
                if path in nodeSet:
                    ctr, ext, R = rt_decode_obb(bulk.node_metadata[i].oriented_bounding_box, head_center, mtt_per_level[rlevel-1])

                    if transformToWGS84:
                        corners0 = np.array((
                            0,0,0,
                            0,0,1,
                            1,0,0,
                            0,1,0)).reshape(4,3)

                        corners = ((corners0 - .5) * 2 * ext[np.newaxis] + ctr) @ R.T
                        T = authalic_to_geodetic_corners(corners)

                        #print('R0\n', R)
                        #print('ctr0', ctr)

                        ext = ext @ T[:3,:3].T / EarthGeodetic.R1
                        ctr = authalic_to_wgs84_pt(ctr) / EarthGeodetic.R1
                        R = T[:3,:3] @ R

                    q = R_to_quat(R)
                    #print('R1\n', R)
                    #print('ctr1', ctr)

                    keybuf = np.zeros(26, dtype=np.uint8) + 255
                    for i in range(len(path)):
                        keybuf[i] = ord(path[i]) if path[i] != 255 else 0
                    # print(keybuf)


                    buf = np.zeros(3+3+4, dtype=np.float32)
                    buf[0:3]  = ctr
                    buf[3:6]  = ext
                    buf[6:10] = q
                    outFp.write(keybuf.tobytes())
                    outFp.write(buf.tobytes())
                    nodesSeen += 1



if __name__ == '__main__':
    with open('/data/gearth/tpAois_wgs/index.v1.bin', 'wb') as fp:
        export_rt_version1(fp, '/data/gearth/tpAois_wgs')