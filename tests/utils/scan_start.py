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
AeroClusterID="1"

if len(sys.argv) != 2:
	print ("Please provide the vmid");
	sys.exit(1)

VmID=sys.argv[1]
headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

# POST call 1 to stord_svc
data1 = {"vmid": "%s" %VmID}
print ("Send POST stord_svc flush_req %s" %VmID)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/scan_del_req/?vm-id=%s" % (h, VmID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

my_list=range(0,10)
for i in my_list:
	print ("[%d:] Send POST stord_svc scan_status %s" %(i, AeroClusterID))
	data = {"aero-cluster-id": "%s" %AeroClusterID}
	r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/scan_status/?aero-cluster-id=%s" % (h, AeroClusterID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
	print(r.text)
	assert (r.status_code == 200)
