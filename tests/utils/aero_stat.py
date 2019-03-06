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

if len(sys.argv) > 1:
    if sys.argv[1].lower() == "https" :
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        h = "https"
        cert=('./cert/cert.pem', './cert/key.pem')

headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

# POST call 1 to stord_svc
data1 = {"vmid": 1}
print ("Send GET stord_svc aero_stat 1")
r = requests.get("%s://127.0.0.1:9000/stord_svc/v1.0/aero_stat/?vm-id=1" % h)
print(r.text)
assert (r.status_code == 200)

print ("Send GET stord_svc vmdk_stats 2")
r = requests.get("%s://127.0.0.1:9000/stord_svc/v1.0/vmdk_stats/?vmdk-id=1" % h)
print(r.json())
assert (r.status_code == 200)
