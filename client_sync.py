import socket
import random,string
from datetime import datetime
from faker import Faker

# host port random byteNum
def echo(host, port, byteNum, request_num):
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        client_socket.connect((host, port))
    except:
        print('can\'t connect to ' + host + ':' + str(port))
        return
    send_data = lambda x: ''.join(random.choices(string.digits + string.ascii_letters, k = x))
    utf8_send_data = send_data(byteNum).encode('utf-8')
    begin = datetime.now()
    for num in range(1, request_num):
        try:
            client_socket.send(utf8_send_data)
            recv_data = client_socket.recv(1024)
        except Exception as e:
            print(e)
            return
    end = datetime.now()
    print(str(request_num * 1000 / (end - begin).microseconds) + ' reques/s')

def main():
    echo('127.0.0.1', 9999, 64, 100000)

if __name__ == '__main__':
    main()