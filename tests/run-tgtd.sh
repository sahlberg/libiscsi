echo "TGTD log: /tmp/tgtd_$$"
/usr/sbin/tgtd -f -e http://127.0.0.1:2379 -s tgt_svc -v v1.0 -p 9001 -D 127.0.0.1 -P 9876 2>&1 | tee -a /tmp/tgtd_$$
