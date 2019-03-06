# PREREQUISITES,
# Need stord to be started with ha port 9000
#./src/stord/./stord -etcd_ip="http://127.0.0.1:2379" -stord_version="v1.0" -svc_label="stord_svc" -ha_svc_port=9000
# Need tgtd to be started with ha port 9001
#./usr/tgtd -f -e "http://127.0.0.1:2379" -s "tgt_svc" -v "v1.0" -p 9001 -D "127.0.0.1" -P 9876

import json
import requests
import time
import sys
import os

from collections import OrderedDict
from urllib.parse import urlencode

h = "http"
cert = None

# Edit AeroSpike details here
AeroClusterIPs="192.168.3.112,192.168.2.24"
AeroClusterPort="3000"
AeroClusterID="1"
TargetName="tgt1"
#TargetName="iqn.test.2006-07.name.target"
TargetName_D="%s" %TargetName

VmId="1"
VmdkID="1"
TargetID="%s" %VmId
LunID="%s" %VmdkID
FileTarget="/tmp/hyc/"
DevTarget="/dev/sdc"

size_in_gb="2" #Size in GB
DevName="iscsi-%s-disk_%s" %(TargetName, LunID)
DevPath="/var/hyc/%s" %(DevName)
print ("DevPath: %s" %DevPath)

if len(sys.argv) > 1:
    if sys.argv[1].lower() == "https" :
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        h = "https"
        cert=('./cert/cert.pem', './cert/key.pem')

headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

# POST call 1 to tgt_svc lun_delete
print ("Send POST tgt_svc lun_delete %s" %TargetName)
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/lun_delete/?tid=%s&lid=%s" % (h, TargetID, LunID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("TGT: LUN %s deleted" %LunID)

# POST call 2 to tgt_svc target_delete
force_delete = 1
print ("Send POST tgt_svc target_delete %s" %TargetName)
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/target_delete/?tid=%s&force=%s" % (h, TargetID, force_delete), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("TGT: target deleted")

# POST call 3 to stord_svc vmdk_delete
print ("Send POST stord_svc vmdk_delete")
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/vmdk_delete/?vm-id=%s&vmdk-id=%s" % (h, VmId, VmdkID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)


'''
print ("Send POST stord_svc del_target")
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/del_target/?vm_id=%s&vmdk_id=%s" % (h, VmId, VmdkID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
'''

# POST call 4 to stord_svc vm_delete
print ("Send POST stord_svc vm_delete %s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/vm_delete/?vm-id=%s" %(h, VmId))
assert (r.status_code == 200)

'''
# POST call 5 to stord_svc aero_set_cleanup
print ("Send POST aero_set_delete %s" %VmId)
data2 = {"vmid": "%s" %VmId, "TargetName":"%s" %TargetName}
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/aero_set_cleanup/?aero-id=%s" % (h, AeroClusterID), data=json.dumps(data2), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("STORD: target set deleted")
'''

# POST call 6 to stord_svc aero_del
print ("Send POST del_aero %s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/del_aero/?aero-id=1" % h, headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

'''
print ("Sleep for 20 seconds - should still be alive")
time.sleep(20)

# Stop component for tgt_svc on port 9001
print ("Post stop command to service")
r = requests.post("%s://127.0.0.1:9001/ha_svc/v1.0/component_stop" % h, data=json.dumps(data), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)

# Stop component for stord_svc on port 9000
print ("Post stop command to service")
r = requests.post("%s://127.0.0.1:9000/ha_svc/v1.0/component_stop" % h, data=json.dumps(data), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)
'''
