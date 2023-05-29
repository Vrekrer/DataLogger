import socket
import os
import time

port = 54321

if __name__ == '__main__':
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('0.0.0.0', port))
        s.listen()
        while True:
            con, addr = s.accept()
            data = b''
            with con:
                while True:
                    d = con.recv(1)
                    if not d: break
                    data += d
                    if d == '\n': break
            data = data.decode("utf-8")
            mac_address = data.split(',')[0]
            file_name = mac_address.replace(':', '_') + '.txt'
            if not os.path.isfile(file_name):
                with open(file_name, 'w') as f:
                    f.write('#Data from sensor'
                            ' with mac address %s\n' % mac_address)
                    f.write('#Human Time, '
                            'Timestamp (s), '
                            'Temperature (°C), '
                            'Relative Humidity (%)\n')
            data = ', '.join(data.split(',')[1:])
            with open(file_name, 'a') as f:
                f.write('%s, %s, %s' % (time.asctime(),
                                        round(time.time()),
                                        data))
            print(time.asctime(), end='\n   ')
            print(mac_address, end='\n   ')
            print(data)
