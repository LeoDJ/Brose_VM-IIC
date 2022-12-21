import socket
import struct
import datetime
import netifaces

multicast_group = '224.1.33.7'
multicast_port = 1234


# get ip address of default network interface (this is needed for binding multicast to the correct network adapter)
# (See also the infamous "VirtualBox Network Adapter problem")
default_gw_device = netifaces.gateways()['default'][netifaces.AF_INET][1]
def_gw_ip = netifaces.ifaddresses(default_gw_device)[netifaces.AF_INET][0]['addr']

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

# Set the socket to be able to reuse the address
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)


# Bind the socket to a specific multicast group and port
sock.bind((def_gw_ip, multicast_port))

# Tell the kernel to add the socket to the multicast group
# on the default interface
mreq = struct.pack('4s4s', socket.inet_aton(multicast_group), socket.inet_aton(def_gw_ip))
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

print(f"Logger running. \nUsing multicast address: {multicast_group}:{multicast_port} \nListening at interface with IP: {def_gw_ip}\n")

# Receive data from the multicast group
while True:
    data, address = sock.recvfrom(1024)
    # print(f'Received {data} from {address}')
    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] {address[0]}:{address[1]} -> {data.decode('utf-8')}", end="")