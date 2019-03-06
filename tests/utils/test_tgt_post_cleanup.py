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
createfile="true"
DevTarget="/dev/sdc"

size_in_gb="16" #Size in GB
DevName="iscsi-%s-disk_%s" %(TargetName, LunID)
DevPath="/var/hyc/%s" %(DevName)
cmd="truncate --size=%sG %s" %(size_in_gb, DevPath)

print ("DevPath: %s" %DevPath)
print ("Cmd: %s" %cmd)
os.system(cmd);

if len(sys.argv) > 1:
    if sys.argv[1].lower() == "https" :
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        h = "https"
        cert=('./cert/cert.pem', './cert/key.pem')

headers = {'Content-type': 'application/json'}
params = OrderedDict([('first', 1), ('second', 2), ('third', 3)])
data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}

'''
# Start component for stord_svc on port 9000
print ("Post start command to service")
r = requests.post("%s://127.0.0.1:9000/ha_svc/v1.0/component_start" % h, data=json.dumps(data), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)
'''

# POST call 1 to stord_svc, Add new aero cluster at StorD
print ("New Aero command...")
data1 = {"aeroid": "%s" %AeroClusterID, "AeroClusterIPs":"%s" %AeroClusterIPs,"AeroClusterPort":"%s" %AeroClusterPort,"AeroClusterID":"%s" %AeroClusterID}
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/new_aero/?aero-id=1" % h, data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

# POST call 2 to stord_svc
print ("New VM command...")
data1 = { "vmid": "%s" %VmId, "TargetID": "%s" %TargetID, "TargetName": "%s" %TargetName, "AeroClusterID":"%s" %AeroClusterID}
print ("Send POST stord_svc new_vm %s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/new_vm/?vm-id=%s" %(h, VmId), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

# POST call 3 to stord_svc
print ("Send POST stord_svc new_vmdk 1")
data2 = {"TargetID":"%s" %TargetID,"LunID":"%s" %LunID,"DevPath":"%s" %DevPath,"VmID":"%s" %VmId, "VmdkID":"%s" %VmdkID,"BlockSize":"4096","Compression":{"Enabled":"false"},"Encryption":{"Enabled":"false"},"RamCache":{"Enabled":"false","MemoryInMB":"1024"},"FileCache":{"Enabled":"false"},"SuccessHandler":{"Enabled":"false"}, "FileTarget":{"Enabled":"false"}}
#data2 = {"TargetID":"%s" %TargetID,"LunID":"%s" %LunID,"DevPath":"%s" %DevPath,"VmID":"%s" %VmId, "VmdkID":"%s" %VmdkID,"BlockSize":"4096","Compression":{"Enabled":"false"},"Encryption":{"Enabled":"false"},"RamCache":{"Enabled":"false","MemoryInMB":"1024"},"FileCache":{"Enabled":"false"},"SuccessHandler":{"Enabled":"false"}, "FileTarget":{"Enabled":"true","CreateFile":"%s" %createfile, "TargetFilePath":"%s" %DevTarget,"TargetFileSize":"17179869184"}}

r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/new_vmdk/?vm-id=%s&vmdk-id=%s" % (h,VmId,VmdkID), data=json.dumps(data2), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

'''
# Start component for tgt_svc on port 9001
print ("Post start command to service")
r = requests.post("%s://127.0.0.1:9001/ha_svc/v1.0/component_start" % h, data=json.dumps(data), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)
print ("TGT: start component done")

# POST call to tgt_svc adding stord to tgt
data3 = { "StordIp": "127.0.0.1", "StordPort": "9876"}
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/new_stord" % (h), data=json.dumps(data3), headers=headers, cert=cert, verify=False)
print (r.text)
assert (r.status_code == 200)
print ("TGT: new stord added")
'''

# POST call 4 to tgt_svc
print ("Send POST tgt_svc target_create %s" %TargetName)
data1 = {"TargetName": "%s" %TargetName}
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/target_create/?tid=%s" % (h, TargetID), data=json.dumps(data1), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

# POST call 5 to tgt_svc
print ("Send POST tgt_svc lun_create %s" %LunID)
data2 = {"DevName": "%s" %(DevName), "VmID":"%s" %VmId, "VmdkID":"%s" %VmdkID, "LunSize":"%s" %size_in_gb}
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/lun_create/?tid=%s&lid=%s" % (h, TargetID, LunID), data=json.dumps(data2), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)

'''
# POST call 6 to tgt_svc lun_delete
print ("Send POST tgt_svc lun_delete %s" %TargetName)
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/lun_delete/?tid=%s&lid=%s" % (h, TargetID, LunID), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("TGT: LUN %s deleted" %LunID)

# POST call 7 to tgt_svc target_delete
force_delete = 1
print ("Send POST tgt_svc target_delete %s" %TargetName)
r = requests.post("%s://127.0.0.1:9001/tgt_svc/v1.0/target_delete/?tid=%s&force=%s" % (h, TargetID, force_delete), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("TGT: target deleted")

# POST call 8 to stord_svc aero_set_delete
print ("Send POST aero_set_delete %s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/aero_set_delete/?vm-id=%s" % (h, VmId), headers=headers, cert=cert, verify=False)
assert (r.status_code == 200)
print ("STORD: target set deleted")

print ("Sleep for 120 seconds - should still be alive")
time.sleep(120)

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

# keepalive stopped - 
print ("Sleep for 120 seconds - lease should expire after 2 mins")
time.sleep(120)
'''
