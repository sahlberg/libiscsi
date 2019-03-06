#!/bin/bash

mkdir -p /tmp/hyc /var/hyc

/usr/sbin/stord -etcd_ip="http://127.0.0.1:2379" -stord_version="v1.0" -svc_label="stord_svc" -ha_svc_port=9000 -v 0
