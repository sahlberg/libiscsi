import json
import requests
import time
import sys
import os

from collections import OrderedDict
from urllib.parse import urlencode

h = "http"
cert = None
VmId="1"
headers = {'Content-type': 'application/json'}

if len(sys.argv) > 1:
    if sys.argv[1].lower() == "https" :
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        h = "https"
        cert=('./cert/cert.pem', './cert/key.pem')

#GetUnflushedCheckpoints
print("\n")
print("===============================================================================")
print("GetUnflushedCheckpoints ...")
print ("Send GET stord_svc/get_unflushed_checkpoints/?vm-id=%s" %VmId)
r = requests.get("%s://127.0.0.1:9000/stord_svc/v1.0/get_unflushed_checkpoints/?vm-id=%s" %(h, VmId))
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 200)

print("\n")

#PrepareFlush 
print("===============================================================================")
print("PrepareFlush ...")
print ("Send POST stord_svc/prepare_flush/?vm-id=%s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/prepare_flush/?vm-id=%s" %(h, VmId))
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 200)

print("\n")

#AsyncStartFlush
print("===============================================================================")
print("AsyncStartFlush ...")
print ("Send POST stord_svc/async_start_flush/?vm-id=%s" %VmId)
ckpt_ids = [41, 42, 43]
data = {"checkpoint-ids": "%s" %ckpt_ids}
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/async_start_flush/?vm-id=%s" 
	%(h, VmId), data=json.dumps(data), headers=headers, cert=cert, verify=False)
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 202)

print("\n")

#GetFlushStatus
print("===============================================================================")
print("GetFlushStatus ...")
r = requests.get("%s://127.0.0.1:9000/stord_svc/v1.0/flush_status/?vm-id=%s" % (h, VmId))
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")

print("\n")

#SerializeCheckpoints
print("===============================================================================")
print("SerializeCheckpoints ...")
print ("Send POST stord_svc/serialize_checkpoints/?vm-id=%s" %VmId)
ckpt_ids = [41, 42, 43]
snapshot_id = 101
data = {"checkpoint-ids": "%s" %ckpt_ids, "snapshot-id": "%s" %snapshot_id}
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/serialize_checkpoints/?vm-id=%s" 
	%(h, VmId), data=json.dumps(data), headers=headers, cert=cert, verify=False)
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 200)

print("\n")

print("===============================================================================")
print("AsyncStartMoveStage ...")
print ("Send GET stord_svc/async_start_move_stage/?vm-id=%s" %VmId)
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/async_start_move_stage/?vm-id=%s" %(h, VmId))
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 202)

print("\n")

#GetMoveStatus
print("===============================================================================")
print("GetMoveStatus ...")
r = requests.get("%s://127.0.0.1:9000/stord_svc/v1.0/move_status/?vm-id=%s" % (h, VmId))
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")

print("\n")

#DeleteSnapshots
print("===============================================================================")
print("DeleteSnapshots ...")
print ("Send POST stord_svc/delete_snapshots/?vm-id=%s" %VmId)
snapshot_ids = [101, 103]
data = {"snapshot-ids": "%s" %snapshot_ids}
r = requests.post("%s://127.0.0.1:9000/stord_svc/v1.0/delete_snapshots/?vm-id=%s" 
	%(h, VmId), data=json.dumps(data), headers=headers, cert=cert, verify=False)
print("HTTP Response: %s" %r.status_code)
print ("Result: %s" %r.json())
print("===============================================================================")
assert (r.status_code == 200)
