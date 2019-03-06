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
NepID="1"
VmID = "1"
VmdkID = "1"
nep_ip="192.168.5.138"
nep_port=8000

if len(sys.argv) > 1:
    if sys.argv[1].lower() == "https" :
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        h = "https"
        cert=('./cert/cert.pem', './cert/key.pem')

headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

# Start component for stord_svc on port 9000
print ("Post start command to service")
r = requests.post("%s://127.0.0.1:9000/ha_svc/v1.0/component_start" % h, data=json.dumps(data), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)

# Add transport
data1 = {"nep_ip": "%s" %(nep_ip), "nep_port": 8000 }
print ("Send add transport nep_req %s" %NepID)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/add_nep_transport/?nep-id=%s" % (h, NepID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

#####################################################################
# add peers in loop
#####################################################################
## POST call 1 to add_peer
add_peer_data = {
    "peer_ip": "192.168.2.164",
    "peer_port": 8001
}

print ("Add peer rest API (POST)")
url = "http://127.0.0.1:9000/stord_svc/v1.0/add_peer/?peer-id=1" 
r = requests.post(
    url,
    data=json.dumps(add_peer_data),
    headers=headers,
    cert=None,
    verify=False)
assert (r.status_code == 200)

# Add target
data1 = {"target_type": "nep", "src_id": 201, "dest_id": 101}
print ("Send add target nep_req")
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/add_target/?vm_id=%s&vmdk_id=%s" % (h, VmID, VmdkID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
