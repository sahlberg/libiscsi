import configparser

configParser = configparser.RawConfigParser()
configFilePath = r'./config.ini'
configParser.read(configFilePath)

AeroClusterIPs = configParser.get("HYC", "AeroClusterIPs")
AeroClusterPort = configParser.get("HYC", "AeroClusterPort")
AeroClusterID = configParser.get("HYC", "AeroClusterID")

StordIp = configParser.get("HYC", "StordIP")
StordPort = configParser.get("HYC", "StordPort")
StordUrl = "%s:%s" %(StordIp, StordPort)

TgtIp = configParser.get("HYC", "TgtIp")
TgtPort = configParser.get("HYC", "TgtPort")
TgtUrl = "%s:%s" %(TgtIp, TgtPort)
TgtToStordPort = configParser.get("HYC", "TgtToStordPort")

EtcdIps = configParser.get("HYC", "EtcdIps")

#All disks will be of below size
size_in_gb=20

TargetNameStr = configParser.get("HYC", "TargetNameStr")

FileTarget = configParser.get("HYC", "FileTarget")
FileSize = size_in_gb << 30

DevTarget = configParser.get("HYC", "DevTarget")
DevTarget = DevTarget.split(",")

TargetType = configParser.get("HYC", "TargetType")

h = "http"
headers = {'Content-type': 'application/json'}

TargetIp = configParser.get("HYC", "TargetIp")
