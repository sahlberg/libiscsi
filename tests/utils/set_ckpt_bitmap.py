# PREREQUISITES,
# Need stord to be started with ha port 9000
#./src/stord/./stord -etcd_ip="http://127.0.0.1:2379" -stord_version="v1.0" -svc_label="stord_svc" -ha_svc_port=9000
# Need tgtd to be started with ha port 9001
#./usr/tgtd -f -e "http://127.0.0.1:2379" -s "tgt_svc" -v "v1.0" -p 9001 -D "127.0.0.1" -P 9876

import json
import requests
import time
import sys

from collections import OrderedDict
from urllib.parse import urlencode

h = "http"
cert = None
VmID="1"
VmdkID="1"
ckptID="1"

if len(sys.argv) > 1:
	VmID=sys.argv[1]

headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/prepare_ckpt/?vm-id=%s" % (h, VmID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

extents="4294967296:104857600,6442450944:104857600"
data1 = {"Extents": "%s"  %(extents)}
print ("Send POST set_bitmap req for vmid:: %s, vmdkid : %s" %(VmID, VmdkID))
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/set_bitmap/?vm-id=%s&vmdk-id=%s" % (h, VmID, VmdkID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/commit_ckpt/?vm-id=%s&ckpt-id=%s" % (h, VmID, ckptID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
